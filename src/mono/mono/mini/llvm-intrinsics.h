
/*
 * List of LLVM intrinsics
 *
 * INTRINS(name, llvm_name)
 */

INTRINS(MEMSET, "llvm.memset.p0i8.i32")
INTRINS(MEMCPY, "llvm.memcpy.p0i8.p0i8.i32")
INTRINS(MEMMOVE, "llvm.memmove.p0i8.p0i8.i64")
INTRINS(SADD_OVF_I32, "llvm.sadd.with.overflow.i32")
INTRINS(UADD_OVF_I32, "llvm.uadd.with.overflow.i32")
INTRINS(SSUB_OVF_I32, "llvm.ssub.with.overflow.i32")
INTRINS(USUB_OVF_I32, "llvm.usub.with.overflow.i32")
INTRINS(SMUL_OVF_I32, "llvm.smul.with.overflow.i32")
INTRINS(UMUL_OVF_I32, "llvm.umul.with.overflow.i32")
INTRINS(SADD_OVF_I64, "llvm.sadd.with.overflow.i64")
INTRINS(UADD_OVF_I64, "llvm.uadd.with.overflow.i64")
INTRINS(SSUB_OVF_I64, "llvm.ssub.with.overflow.i64")
INTRINS(USUB_OVF_I64, "llvm.usub.with.overflow.i64")
INTRINS(SMUL_OVF_I64, "llvm.smul.with.overflow.i64")
INTRINS(UMUL_OVF_I64, "llvm.umul.with.overflow.i64")
INTRINS(SIN, "llvm.sin.f64")
INTRINS(COS, "llvm.cos.f64")
INTRINS(SQRT, "llvm.sqrt.f64")
INTRINS(FLOOR, "llvm.floor.f64")
INTRINS(FLOORF, "llvm.floor.f32")
INTRINS(CEIL, "llvm.ceil.f64")
INTRINS(CEILF, "llvm.ceil.f32")
INTRINS(FMA, "llvm.fma.f64")
INTRINS(FMAF, "llvm.fma.f32")
	/* This isn't an intrinsic, instead llvm seems to special case it by name */
INTRINS(FABS, "fabs")
INTRINS(ABSF, "llvm.fabs.f32")
INTRINS(SINF, "llvm.sin.f32")
INTRINS(COSF, "llvm.cos.f32")
INTRINS(SQRTF, "llvm.sqrt.f32")
INTRINS(POWF, "llvm.pow.f32")
INTRINS(POW, "llvm.pow.f64")
INTRINS(EXP, "llvm.exp.f64")
INTRINS(EXPF, "llvm.exp.f32")
INTRINS(LOG, "llvm.log.f64")
INTRINS(LOG2, "llvm.log2.f64")
INTRINS(LOG2F, "llvm.log2.f32")
INTRINS(LOG10, "llvm.log10.f64")
INTRINS(LOG10F, "llvm.log10.f32")
INTRINS(TRUNC, "llvm.trunc.f64")
INTRINS(TRUNCF, "llvm.trunc.f32")
INTRINS(COPYSIGN, "llvm.copysign.f64")
INTRINS(COPYSIGNF, "llvm.copysign.f32")
INTRINS(EXPECT_I8, "llvm.expect.i8")
INTRINS(EXPECT_I1, "llvm.expect.i1")
INTRINS(CTPOP_I32, "llvm.ctpop.i32")
INTRINS(CTPOP_I64, "llvm.ctpop.i64")
INTRINS(CTLZ_I32, "llvm.ctlz.i32")
INTRINS(CTLZ_I64, "llvm.ctlz.i64")
INTRINS(CTTZ_I32, "llvm.cttz.i32")
INTRINS(CTTZ_I64, "llvm.cttz.i64")
INTRINS(BZHI_I32, "llvm.x86.bmi.bzhi.32")
INTRINS(BZHI_I64, "llvm.x86.bmi.bzhi.64")
INTRINS(BEXTR_I32, "llvm.x86.bmi.bextr.32")
INTRINS(BEXTR_I64, "llvm.x86.bmi.bextr.64")
INTRINS(PEXT_I32, "llvm.x86.bmi.pext.32")
INTRINS(PEXT_I64, "llvm.x86.bmi.pext.64")
INTRINS(PDEP_I32, "llvm.x86.bmi.pdep.32")
INTRINS(PDEP_I64, "llvm.x86.bmi.pdep.64")
#if defined(TARGET_AMD64) || defined(TARGET_X86)
INTRINS(SSE_PMOVMSKB, "llvm.x86.sse2.pmovmskb.128")
INTRINS(SSE_MOVMSK_PS, "llvm.x86.sse.movmsk.ps")
INTRINS(SSE_MOVMSK_PD, "llvm.x86.sse2.movmsk.pd")
INTRINS(SSE_PSRLI_W, "llvm.x86.sse2.psrli.w")
INTRINS(SSE_PSRAI_W, "llvm.x86.sse2.psrai.w")
INTRINS(SSE_PSLLI_W, "llvm.x86.sse2.pslli.w")
INTRINS(SSE_PSRLI_D, "llvm.x86.sse2.psrli.d")
INTRINS(SSE_PSRAI_D, "llvm.x86.sse2.psrai.d")
INTRINS(SSE_PSLLI_D, "llvm.x86.sse2.pslli.d")
INTRINS(SSE_PSRLI_Q, "llvm.x86.sse2.psrli.q")
INTRINS(SSE_PSLLI_Q, "llvm.x86.sse2.pslli.q")
INTRINS(SSE_SQRT_PD, "llvm.x86.sse2.sqrt.pd")
INTRINS(SSE_SQRT_PS, "llvm.x86.sse.sqrt.ps")
INTRINS(SSE_RSQRT_PS, "llvm.x86.sse.rsqrt.ps")
INTRINS(SSE_RCP_PS, "llvm.x86.sse.rcp.ps")
INTRINS(SSE_CVTTPD2DQ, "llvm.x86.sse2.cvttpd2dq")
INTRINS(SSE_CVTTPS2DQ, "llvm.x86.sse2.cvttps2dq")
INTRINS(SSE_CVTDQ2PD, "llvm.x86.sse2.cvtdq2pd")
INTRINS(SSE_CVTDQ2PS, "llvm.x86.sse2.cvtdq2ps")
INTRINS(SSE_CVTPD2DQ, "llvm.x86.sse2.cvtpd2dq")
INTRINS(SSE_CVTPS2DQ, "llvm.x86.sse2.cvtps2dq")
INTRINS(SSE_CVTPD2PS, "llvm.x86.sse2.cvtpd2ps")
INTRINS(SSE_CVTPS2PD, "llvm.x86.sse2.cvtps2pd")
INTRINS(SSE_CVTSS2SI, "llvm.x86.sse.cvtss2si")
INTRINS(SSE_CVTSS2SI64, "llvm.x86.sse.cvtss2si64")
INTRINS(SSE_CVTTSS2SI, "llvm.x86.sse.cvttss2si")
INTRINS(SSE_CVTTSS2SI64, "llvm.x86.sse.cvttss2si64")
INTRINS(SSE_CVTSD2SI, "llvm.x86.sse2.cvtsd2si")
INTRINS(SSE_CVTSD2SI64, "llvm.x86.sse2.cvtsd2si64")
INTRINS(SSE_CVTTSD2SI64, "llvm.x86.sse2.cvttsd2si64")
INTRINS(SSE_CVTSI2SS, "llvm.x86.sse.cvtsi2ss")
INTRINS(SSE_CVTSI2SS64, "llvm.x86.sse.cvtsi642ss")
INTRINS(SSE_CVTSI2SD, "llvm.x86.sse2.cvtsi2sd")
INTRINS(SSE_CVTSI2SD64, "llvm.x86.sse2.cvtsi642sd")
INTRINS(SSE_CMPPD, "llvm.x86.sse2.cmp.pd")
INTRINS(SSE_CMPPS, "llvm.x86.sse.cmp.ps")
INTRINS(SSE_PACKSSWB, "llvm.x86.sse2.packsswb.128")
INTRINS(SSE_PACKUSWB, "llvm.x86.sse2.packuswb.128")
INTRINS(SSE_PACKSSDW, "llvm.x86.sse2.packssdw.128")
INTRINS(SSE_PACKUSDW, "llvm.x86.sse41.packusdw")
INTRINS(SSE_MINPS, "llvm.x86.sse.min.ps")
INTRINS(SSE_MAXPS, "llvm.x86.sse.max.ps")
INTRINS(SSE_HADDPS, "llvm.x86.sse3.hadd.ps")
INTRINS(SSE_HSUBPS, "llvm.x86.sse3.hsub.ps")
INTRINS(SSE_ADDSUBPS, "llvm.x86.sse3.addsub.ps")
INTRINS(SSE_MINPD, "llvm.x86.sse2.min.pd")
INTRINS(SSE_MAXPD, "llvm.x86.sse2.max.pd")
INTRINS(SSE_HADDPD, "llvm.x86.sse3.hadd.pd")
INTRINS(SSE_HSUBPD, "llvm.x86.sse3.hsub.pd")
INTRINS(SSE_ADDSUBPD, "llvm.x86.sse3.addsub.pd")
INTRINS(SSE_PADDSW, "llvm.x86.sse2.padds.w")
INTRINS(SSE_PSUBSW, "llvm.x86.sse2.psubs.w")
INTRINS(SSE_PADDUSW, "llvm.x86.sse2.paddus.w")
INTRINS(SSE_PSUBUSW, "llvm.x86.sse2.psubus.w")
INTRINS(SSE_PAVGW, "llvm.x86.sse2.pavg.w")
INTRINS(SSE_PMULHW, "llvm.x86.sse2.pmulh.w")
INTRINS(SSE_PMULHU, "llvm.x86.sse2.pmulhu.w")
INTRINS(SE_PADDSB, "llvm.x86.sse2.padds.b")
INTRINS(SSE_PSUBSB, "llvm.x86.sse2.psubs.b")
INTRINS(SSE_PADDUSB, "llvm.x86.sse2.paddus.b")
INTRINS(SSE_PSUBUSB, "llvm.x86.sse2.psubus.b")
INTRINS(SSE_PAVGB, "llvm.x86.sse2.pavg.b")
INTRINS(SSE_PAUSE, "llvm.x86.sse2.pause")
INTRINS(SSE_PSHUFB, "llvm.x86.ssse3.pshuf.b.128")
INTRINS(SSE_DPPS, "llvm.x86.sse41.dpps")
INTRINS(SSE_ROUNDSS, "llvm.x86.sse41.round.ss")
INTRINS(SSE_ROUNDPD, "llvm.x86.sse41.round.pd")
INTRINS(SSE_PTESTZ, "llvm.x86.sse41.ptestz")
INTRINS(SSE_INSERTPS, "llvm.x86.sse41.insertps")
#if LLVM_API_VERSION >= 800
	// these intrinsics were renamed in LLVM 8
INTRINS(SSE_SADD_SATI8, "llvm.sadd.sat.v16i8")
INTRINS(SSE_UADD_SATI8, "llvm.uadd.sat.v16i8")
INTRINS(SSE_SADD_SATI16, "llvm.sadd.sat.v8i16")
INTRINS(SSE_UADD_SATI16, "llvm.uadd.sat.v8i16")
#else
INTRINS(SSE_SADD_SATI8, "llvm.x86.sse2.padds.b")
INTRINS(SSE_UADD_SATI8, "llvm.x86.sse2.paddus.b")
INTRINS(SSE_SADD_SATI16, "llvm.x86.sse2.padds.w")
INTRINS(SSE_UADD_SATI16, "llvm.x86.sse2.paddus.w")
#endif
#endif
#ifdef TARGET_WASM
INTRINS(WASM_ANYTRUE_V16, "llvm.wasm.anytrue.v16i8")
INTRINS(WASM_ANYTRUE_V8, "llvm.wasm.anytrue.v8i16")
INTRINS(WASM_ANYTRUE_V4, "llvm.wasm.anytrue.v4i32")
INTRINS(WASM_ANYTRUE_V2, "llvm.wasm.anytrue.v2i64")
#endif

#undef INTRINS

