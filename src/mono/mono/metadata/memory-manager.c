#include <mono/metadata/loader-internals.h>
#include <mono/metadata/gc-internals.h>
#include <mono/metadata/reflection-cache.h>
#include <mono/metadata/mono-hash-internals.h>
#include <mono/metadata/debug-internals.h>
#include <mono/utils/unlocked.h>

static GENERATE_GET_CLASS_WITH_CACHE (loader_allocator, "System.Reflection", "LoaderAllocator");

static LockFreeMempool*
lock_free_mempool_new (void)
{
	return g_new0 (LockFreeMempool, 1);
}

static void
lock_free_mempool_free (LockFreeMempool *mp)
{
	LockFreeMempoolChunk *chunk, *next;

	chunk = mp->chunks;
	while (chunk) {
		next = (LockFreeMempoolChunk *)chunk->prev;
		mono_vfree (chunk, mono_pagesize (), MONO_MEM_ACCOUNT_DOMAIN);
		chunk = next;
	}
	g_free (mp);
}

/*
 * This is async safe
 */
static LockFreeMempoolChunk*
lock_free_mempool_chunk_new (LockFreeMempool *mp, int len)
{
	LockFreeMempoolChunk *chunk, *prev;
	int size;

	size = mono_pagesize ();
	while (size - sizeof (LockFreeMempoolChunk) < len)
		size += mono_pagesize ();
	chunk = (LockFreeMempoolChunk *)mono_valloc (0, size, MONO_MMAP_READ|MONO_MMAP_WRITE, MONO_MEM_ACCOUNT_DOMAIN);
	g_assert (chunk);
	chunk->mem = (guint8 *)ALIGN_PTR_TO ((char*)chunk + sizeof (LockFreeMempoolChunk), 16);
	chunk->size = ((char*)chunk + size) - (char*)chunk->mem;
	chunk->pos = 0;

	/* Add to list of chunks lock-free */
	while (TRUE) {
		prev = mp->chunks;
		if (mono_atomic_cas_ptr ((volatile gpointer*)&mp->chunks, chunk, prev) == prev)
			break;
	}
	chunk->prev = prev;

	return chunk;
}

/*
 * This is async safe
 */
static gpointer
lock_free_mempool_alloc0 (LockFreeMempool *mp, guint size)
{
	LockFreeMempoolChunk *chunk;
	gpointer res;
	int oldpos;

	// FIXME: Free the allocator

	size = ALIGN_TO (size, 8);
	chunk = mp->current;
	if (!chunk) {
		chunk = lock_free_mempool_chunk_new (mp, size);
		mono_memory_barrier ();
		/* Publish */
		mp->current = chunk;
	}

	/* The code below is lock-free, 'chunk' is shared state */
	oldpos = mono_atomic_fetch_add_i32 (&chunk->pos, size);
	if (oldpos + size > chunk->size) {
		chunk = lock_free_mempool_chunk_new (mp, size);
		g_assert (chunk->pos + size <= chunk->size);
		res = chunk->mem;
		chunk->pos += size;
		mono_memory_barrier ();
		mp->current = chunk;
	} else {
		res = (char*)chunk->mem + oldpos;
	}

	return res;
}

static void
memory_manager_init (MonoMemoryManager *memory_manager, gboolean collectible)
{
	MonoDomain *domain = mono_get_root_domain ();

	memory_manager->collectible = collectible;
	memory_manager->freeing = FALSE;

	mono_coop_mutex_init_recursive (&memory_manager->lock);
	mono_os_mutex_init (&memory_manager->mp_mutex);

	memory_manager->_mp = mono_mempool_new ();
	memory_manager->code_mp = mono_code_manager_new ();
	memory_manager->lock_free_mp = lock_free_mempool_new ();

	memory_manager->class_vtable_array = g_ptr_array_new ();

	// TODO: make these not linked to the domain for debugging
	memory_manager->type_hash = mono_g_hash_table_new_type_internal ((GHashFunc)mono_metadata_type_hash, (GCompareFunc)mono_metadata_type_equal, MONO_HASH_VALUE_GC, MONO_ROOT_SOURCE_DOMAIN, domain, "Domain Reflection Type Table");
	memory_manager->refobject_hash = mono_conc_g_hash_table_new_type (mono_reflected_hash, mono_reflected_equal, MONO_HASH_VALUE_GC, MONO_ROOT_SOURCE_DOMAIN, domain, "Domain Reflection Object Table");
	memory_manager->type_init_exception_hash = mono_g_hash_table_new_type_internal (mono_aligned_addr_hash, NULL, MONO_HASH_VALUE_GC, MONO_ROOT_SOURCE_DOMAIN, domain, "Domain Type Initialization Exception Table");

	if (mono_get_runtime_callbacks ()->init_mem_manager)
		mono_get_runtime_callbacks ()->init_mem_manager (memory_manager);
}

MonoSingletonMemoryManager *
mono_mem_manager_create_singleton (MonoAssemblyLoadContext *alc, gboolean collectible)
{
	MonoSingletonMemoryManager *mem_manager = g_new0 (MonoSingletonMemoryManager, 1);
	memory_manager_init ((MonoMemoryManager *)mem_manager, collectible);

	mem_manager->memory_manager.is_generic = FALSE;
	mem_manager->alc = alc;

	return mem_manager;
}

static void
cleanup_refobject_hash (gpointer key, gpointer value, gpointer user_data)
{
	free_reflected_entry ((ReflectedEntry *)key);
}

static void
unregister_vtable_reflection_type (MonoVTable *vtable)
{
	MonoObject *type = (MonoObject *)vtable->type;

	if (type->vtable->klass != mono_defaults.runtimetype_class)
		MONO_GC_UNREGISTER_ROOT_IF_MOVING (vtable->type);
}

// First phase of deletion
static void
memory_manager_delete_objects (MonoMemoryManager *memory_manager)
{
	memory_manager->freeing = TRUE;

	// Must be done before type_hash is freed
	for (int i = 0; i < memory_manager->class_vtable_array->len; i++)
		unregister_vtable_reflection_type ((MonoVTable *)g_ptr_array_index (memory_manager->class_vtable_array, i));

	g_ptr_array_free (memory_manager->class_vtable_array, TRUE);
	memory_manager->class_vtable_array = NULL;
	mono_g_hash_table_destroy (memory_manager->type_hash);
	memory_manager->type_hash = NULL;
	mono_conc_g_hash_table_foreach (memory_manager->refobject_hash, cleanup_refobject_hash, NULL);
	mono_conc_g_hash_table_destroy (memory_manager->refobject_hash);
	memory_manager->refobject_hash = NULL;
	mono_g_hash_table_destroy (memory_manager->type_init_exception_hash);
	memory_manager->type_init_exception_hash = NULL;
}

// Full deletion
static void
memory_manager_delete (MonoMemoryManager *memory_manager, gboolean debug_unload)
{
	// Scan here to assert no lingering references in vtables?

	if (mono_get_runtime_callbacks ()->free_mem_manager)
		mono_get_runtime_callbacks ()->free_mem_manager (memory_manager);

	if (memory_manager->debug_info) {
		mono_mem_manager_free_debug_info (memory_manager);
		memory_manager->debug_info = NULL;
	}

	if (!memory_manager->freeing)
		memory_manager_delete_objects (memory_manager);

	mono_coop_mutex_destroy (&memory_manager->lock);

	if (debug_unload) {
		mono_mempool_invalidate (memory_manager->_mp);
		mono_code_manager_invalidate (memory_manager->code_mp);
	} else {
#ifndef DISABLE_PERFCOUNTERS
		/* FIXME: use an explicit subtraction method as soon as it's available */
		mono_atomic_fetch_add_i32 (&mono_perfcounters->loader_bytes, -1 * mono_mempool_get_allocated (memory_manager->_mp));
#endif
		mono_mempool_destroy (memory_manager->_mp);
		memory_manager->_mp = NULL;
		mono_code_manager_destroy (memory_manager->code_mp);
		memory_manager->code_mp = NULL;
	}
}

void
mono_mem_manager_free_objects_singleton (MonoSingletonMemoryManager *memory_manager)
{
	g_assert (!memory_manager->memory_manager.freeing);

	memory_manager_delete_objects (&memory_manager->memory_manager);
}

void
mono_mem_manager_free_singleton (MonoSingletonMemoryManager *memory_manager, gboolean debug_unload)
{
	g_assert (!memory_manager->memory_manager.is_generic);

	memory_manager_delete (&memory_manager->memory_manager, debug_unload);
	g_free (memory_manager);
}

void
mono_mem_manager_lock (MonoMemoryManager *memory_manager)
{
	mono_locks_coop_acquire (&memory_manager->lock, MemoryManagerLock);
}

void
mono_mem_manager_unlock (MonoMemoryManager *memory_manager)
{
	mono_locks_coop_release (&memory_manager->lock, MemoryManagerLock);
}

static inline void
alloc_lock (MonoMemoryManager *memory_manager)
{
	mono_os_mutex_lock (&memory_manager->mp_mutex);
}

static inline void
alloc_unlock (MonoMemoryManager *memory_manager)
{
	mono_os_mutex_unlock (&memory_manager->mp_mutex);
}

void *
mono_mem_manager_alloc (MonoMemoryManager *memory_manager, guint size)
{
	void *res;

	g_assert (!memory_manager->frozen);

	alloc_lock (memory_manager);
#ifndef DISABLE_PERFCOUNTERS
	mono_atomic_fetch_add_i32 (&mono_perfcounters->loader_bytes, size);
#endif
	res = mono_mempool_alloc (memory_manager->_mp, size);
	alloc_unlock (memory_manager);

	return res;
}

void *
mono_mem_manager_alloc0 (MonoMemoryManager *memory_manager, guint size)
{
	void *res;

	g_assert (!memory_manager->frozen);

	alloc_lock (memory_manager);
#ifndef DISABLE_PERFCOUNTERS
	mono_atomic_fetch_add_i32 (&mono_perfcounters->loader_bytes, size);
#endif
	res = mono_mempool_alloc0 (memory_manager->_mp, size);
	alloc_unlock (memory_manager);

	return res;
}

char*
mono_mem_manager_strdup (MonoMemoryManager *memory_manager, const char *s)
{
	char *res;

	g_assert (!memory_manager->frozen);

	alloc_lock (memory_manager);
	res = mono_mempool_strdup (memory_manager->_mp, s);
	alloc_unlock (memory_manager);

	return res;
}

gboolean
mono_mem_manager_mp_contains_addr (MonoMemoryManager *memory_manager, gpointer addr)
{
	gboolean res;

	alloc_lock (memory_manager);
	res = mono_mempool_contains_addr (memory_manager->_mp, addr);
	alloc_unlock (memory_manager);
	return res;
}

void *
(mono_mem_manager_code_reserve) (MonoMemoryManager *memory_manager, int size)
{
	void *res;

	mono_mem_manager_lock (memory_manager);
	res = mono_code_manager_reserve (memory_manager->code_mp, size);
	mono_mem_manager_unlock (memory_manager);

	return res;
}

void *
mono_mem_manager_code_reserve_align (MonoMemoryManager *memory_manager, int size, int alignment)
{
	void *res;

	mono_mem_manager_lock (memory_manager);
	res = mono_code_manager_reserve_align (memory_manager->code_mp, size, alignment);
	mono_mem_manager_unlock (memory_manager);

	return res;
}

void
mono_mem_manager_code_commit (MonoMemoryManager *memory_manager, void *data, int size, int newsize)
{
	mono_mem_manager_lock (memory_manager);
	mono_code_manager_commit (memory_manager->code_mp, data, size, newsize);
	mono_mem_manager_unlock (memory_manager);
}

/*
 * mono_mem_manager_code_foreach:
 * Iterate over the code thunks of the code manager of @memory_manager.
 *
 * The @func callback MUST not take any locks. If it really needs to, it must respect
 * the locking rules of the runtime: http://www.mono-project.com/Mono:Runtime:Documentation:ThreadSafety
 * LOCKING: Acquires the memory manager lock.
 */
void
mono_mem_manager_code_foreach (MonoMemoryManager *memory_manager, MonoCodeManagerFunc func, void *user_data)
{
	mono_mem_manager_lock (memory_manager);
	mono_code_manager_foreach (memory_manager->code_mp, func, user_data);
	mono_mem_manager_unlock (memory_manager);
}

gpointer
(mono_mem_manager_alloc0_lock_free) (MonoMemoryManager *memory_manager, guint size)
{
	return lock_free_mempool_alloc0 (memory_manager->lock_free_mp, size);
}

//107, 131, 163
#define HASH_TABLE_SIZE 163
static MonoGenericMemoryManager *mem_manager_cache [HASH_TABLE_SIZE];
static gint32 mem_manager_cache_hit, mem_manager_cache_miss;

static guint32
mix_hash (uintptr_t source)
{
	unsigned int hash = source;

	// Actual hash
	hash = (((hash * 215497) >> 16) ^ ((hash * 1823231) + hash));

	// Mix in highest bits on 64-bit systems only
	if (sizeof (source) > 4)
		hash = hash ^ ((source >> 31) >> 1);

	return hash;
}

static guint32
hash_alcs (MonoAssemblyLoadContext **alcs, int nalcs)
{
	guint32 res = 0;
	int i;
	for (i = 0; i < nalcs; ++i)
		res += mix_hash ((size_t)alcs [i]);

	return res;
}

static gboolean
match_mem_manager (MonoGenericMemoryManager *mm, MonoAssemblyLoadContext **alcs, int nalcs)
{
	int j, k;

	if (mm->n_alcs != nalcs)
		return FALSE;
	/* The order might differ so check all pairs */
	for (j = 0; j < nalcs; ++j) {
		for (k = 0; k < nalcs; ++k)
			if (mm->alcs [k] == alcs [j])
				break;
		if (k == nalcs)
			/* Not found */
			break;
	}

	return j == nalcs;
}


static MonoGenericMemoryManager*
mem_manager_cache_get (MonoAssemblyLoadContext **alcs, int nalcs)
{
	guint32 hash_code = hash_alcs (alcs, nalcs);
	int index = hash_code % HASH_TABLE_SIZE;
	MonoGenericMemoryManager *mm = mem_manager_cache [index];
	if (!mm || !match_mem_manager (mm, alcs, nalcs)) {
		UnlockedIncrement (&mem_manager_cache_miss);
		return NULL;
	}
	UnlockedIncrement (&mem_manager_cache_hit);
	return mm;
}

static void
mem_manager_cache_add (MonoGenericMemoryManager *mem_manager)
{
	guint32 hash_code = hash_alcs (mem_manager->alcs, mem_manager->n_alcs);
	int index = hash_code % HASH_TABLE_SIZE;
	mem_manager_cache [index] = mem_manager;
}

static MonoGenericMemoryManager*
get_mem_manager_for_alcs (MonoAssemblyLoadContext **alcs, int nalcs)
{
	MonoAssemblyLoadContext *alc;
	MonoAssemblyLoadContext *tmp_alcs [1];
	GPtrArray *mem_managers;
	MonoGenericMemoryManager *res;

	/* Can happen for dynamic images */
	if (nalcs == 0) {
		nalcs = 1;
		tmp_alcs [0] = mono_alc_get_default ();
		alcs = tmp_alcs;
	}

	/* Common case */
	if (nalcs == 1 && alcs [0]->generic_memory_manager)
		return alcs [0]->generic_memory_manager;

	// Check in a lock free cache
	res = mem_manager_cache_get (alcs, nalcs);
	if (res)
		return res;

	/*
	 * Find an existing mem manager for these ALCs.
	 * This can exist even if the cache lookup fails since the cache is very simple.
	 */

	/* We can search any ALC in the list, use the first one for now */
	alc = alcs [0];

	mono_alc_memory_managers_lock (alc);

	mem_managers = alc->generic_memory_managers;

	res = NULL;
	for (int mindex = 0; mindex < mem_managers->len; ++mindex) {
		MonoGenericMemoryManager *mm = (MonoGenericMemoryManager*)g_ptr_array_index (mem_managers, mindex);

		if (match_mem_manager (mm, alcs, nalcs)) {
			res = mm;
			break;
		}
	}

	mono_alc_memory_managers_unlock (alc);

	if (res)
		return res;

	/* Create new mem manager */
	res = g_new0 (MonoGenericMemoryManager, 1);
	memory_manager_init ((MonoMemoryManager *)res, FALSE);

	res->memory_manager.is_generic = TRUE;
	res->n_alcs = nalcs;
	res->alcs = mono_mempool_alloc (res->memory_manager._mp, nalcs * sizeof (MonoAssemblyLoadContext*));
	memcpy (res->alcs, alcs, nalcs * sizeof (MonoAssemblyLoadContext*));
	/* The hashes are lazily inited in metadata.c */

	/* Register it into its ALCs */
	for (int i = 0; i < nalcs; ++i) {
		mono_alc_memory_managers_lock (alcs [i]);
		g_ptr_array_add (alcs [i]->generic_memory_managers, res);
		mono_alc_memory_managers_unlock (alcs [i]);
	}

	mono_memory_barrier ();

	mem_manager_cache_add (res);

	if (nalcs == 1 && !alcs [0]->generic_memory_manager)
		alcs [0]->generic_memory_manager = res;

	return res;
}

/*
 * mono_mem_manager_get_generic:
 *
 *   Return a memory manager for allocating memory owned by the set of IMAGES.
 */
MonoGenericMemoryManager*
mono_mem_manager_get_generic (MonoImage **images, int nimages)
{
	MonoAssemblyLoadContext **alcs = g_newa (MonoAssemblyLoadContext*, nimages);
	int nalcs, j;

	/* Collect the set of ALCs owning the images */
	nalcs = 0;
	for (int i = 0; i < nimages; ++i) {
		MonoAssemblyLoadContext *alc = mono_image_get_alc (images [i]);

		if (!alc)
			continue;

		/* O(n^2), but shouldn't be a problem in practice */
		for (j = 0; j < nalcs; ++j)
			if (alcs [j] == alc)
				break;
		if (j == nalcs)
			alcs [nalcs ++] = alc;
	}

	return get_mem_manager_for_alcs (alcs, nalcs);
}

/*
 * mono_mem_manager_merge:
 *
 *   Return a mem manager which depends on the ALCs of MM1/MM2.
 */
MonoGenericMemoryManager*
mono_mem_manager_merge (MonoGenericMemoryManager *mm1, MonoGenericMemoryManager *mm2)
{
	MonoAssemblyLoadContext **alcs = g_newa (MonoAssemblyLoadContext*, mm1->n_alcs + mm2->n_alcs);

	memcpy (alcs, mm1->alcs, sizeof (MonoAssemblyLoadContext*) * mm1->n_alcs);

	int nalcs = mm1->n_alcs;
	/* O(n^2), but shouldn't be a problem in practice */
	for (int i = 0; i < mm2->n_alcs; ++i) {
		int j;
		for (j = 0; j < mm1->n_alcs; ++j) {
			if (mm2->alcs [i] == mm1->alcs [j])
				break;
		}
		if (j == mm1->n_alcs)
			alcs [nalcs ++] = mm2->alcs [i];
	}
	return get_mem_manager_for_alcs (alcs, nalcs);
}

/*
 * Return a GC handle for the LoaderAllocator object
 * which corresponds to the mem manager.
 * Return NULL if the mem manager is not collectible.
 * This is done lazily since mem managers can be created from metadata code.
 */
MonoGCHandle
mono_mem_manager_get_loader_alloc (MonoMemoryManager *mem_manager)
{
	ERROR_DECL (error);
	MonoGCHandle res;

	if (!mem_manager->collectible)
		return NULL;

	if (mem_manager->loader_allocator_weak_handle)
		return mem_manager->loader_allocator_weak_handle;

	/*
	 * Create the LoaderAllocator object which is used to detect whenever there are managed
	 * references to the ALC.
	 */

	/* Try to do most of the construction outside the lock */

	MonoObject *loader_alloc = mono_object_new_pinned (mono_class_get_loader_allocator_class (), error);
	mono_error_assert_ok (error);

	/* This will keep the object alive until unload has started */
	mem_manager->loader_allocator_handle = mono_gchandle_new_internal (loader_alloc, TRUE);

	MonoMethod *method = mono_class_get_method_from_name_checked (mono_class_get_loader_allocator_class (), ".ctor", 1, 0, error);
	mono_error_assert_ok (error);
	g_assert (method);

	/* loader_alloc is pinned */
	gpointer params [1] = { &mem_manager };
	mono_runtime_invoke_checked (method, loader_alloc, params, error);
	mono_error_assert_ok (error);

	mono_mem_manager_lock (mem_manager);
	res = mem_manager->loader_allocator_weak_handle;
	if (!res) {
		res = mono_gchandle_new_weakref_internal (loader_alloc, TRUE);
		mono_memory_barrier ();
		mem_manager->loader_allocator_weak_handle = res;
	} else {
		/* FIXME: The LoaderAllocator object has a finalizer, which shouldn't execute */
	}
	mono_mem_manager_unlock (mem_manager);

	return res;
}
