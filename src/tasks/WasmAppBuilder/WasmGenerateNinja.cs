// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Reflection;
using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;

public class WasmGenerateNinja : Task
{
    [NotNull]
    [Required]
    public string? BinDir { get; set; }
    [NotNull]
    [Required]
    public string? BuildDir { get; set; }
    [NotNull]
    [Required]
    public string? EmsdkPath { get; set; }
    [NotNull]
    [Required]
    public string? EmccCFlags { get; set; }
    [NotNull]
    [Required]
    public string? EmccLDFlags { get; set; }
    [NotNull]
    [Required]
    public string? AOTCompilerPath { get; set; }
    [NotNull]
    [Required]
    public ITaskItem[]? JSSources { get; set; }
    [NotNull]
    [Required]
    public ITaskItem[]? WasmObjects { get; set; }
    [NotNull]
    [Required]
    public ITaskItem[]? AOTAssemblies { get; set; }

    public override bool Execute ()
    {
#if FALSE
        foreach (var item in AOTAssemblies) {
            Console.WriteLine ("I: " + item);
            Console.WriteLine ("I2: " + item.GetMetadata("AotArgs"));
            Console.WriteLine ("I3: " + item.GetMetadata("ProcessArgs"));
        }
#endif

        string builddir = BuildDir!;

        string filename = Path.Combine (builddir, "build.ninja");
        //Console.WriteLine ("Generating '" + filename + "'...");

        // Use O0 for building the final executable to speed up linking
        // The .bc files are still built using -Oz
        EmccCFlags = EmccCFlags.Replace ("-emit-llvm ", "");
        EmccCFlags = EmccCFlags.Replace ("---profiling-funcs ", "");
        EmccLDFlags = EmccLDFlags.Replace ("-Oz", "-O0");
        EmccLDFlags = EmccLDFlags.Replace ("--llvm-opts 2", "-g4");

        var emccFlags = "-g -s DISABLE_EXCEPTION_CATCHING=0 -Oz";

        string ofiles = "";

        // Previous build steps overwrite build inputs even if they have not changed, so make copies

        // Maps original build inputs to real inputs
        var infiles = new Dictionary<string, string> ();
        foreach (var item in JSSources)
            infiles [item.ItemSpec] = Path.Combine ("in", Path.GetFileName (item.ItemSpec));
        foreach (var item in WasmObjects)
            infiles [item.ItemSpec] = Path.Combine ("in", Path.GetFileName (item.ItemSpec));
        foreach (var item in AOTAssemblies)
            infiles [item.ItemSpec] = Path.Combine ("in", Path.GetFileName (item.ItemSpec));

        string[] jsFileNames = JSSources.Select (s => infiles [s.ItemSpec]).ToArray ();
        string jsSources = string.Join (" ", jsFileNames.Select (s => $"--js-library {s}").ToArray ());
        string jsDeps = string.Join (" ", jsFileNames);
        string wasmObjects = string.Join (" ", WasmObjects.Select (s => infiles [s.ItemSpec]).ToArray ());

        ofiles = wasmObjects;

        var ninja = File.CreateText (filename);

        // Defines
        ninja.WriteLine ($"appdir = {BinDir}");
        ninja.WriteLine ("builddir = .");
        ninja.WriteLine ($"emsdk_dir = {EmsdkPath!}");
        ninja.WriteLine ("emsdk_env = $emsdk_dir/emsdk_env.sh");
        ninja.WriteLine ("emcc = source $emsdk_env 2> /dev/null && emcc");
        ninja.WriteLine ($"emcc_flags = {emccFlags}");
        ninja.WriteLine ($"emcc_cflags = {EmccCFlags}");
        ninja.WriteLine ($"emcc_ldflags = {EmccLDFlags}");

        // Rules
        ninja.WriteLine ("rule mkdir");
        ninja.WriteLine ("  command = mkdir -p $out");
        ninja.WriteLine ("rule cp");
        ninja.WriteLine ("  command = cp $in $out");
        // Copy $in to $out only if it changed
        ninja.WriteLine ("rule cpifdiff");
        ninja.WriteLine ("  command = if cmp -s $in $out ; then : ; else cp $in $out ; fi");
        ninja.WriteLine ("  restat = true");
        ninja.WriteLine ("  description = [CPIFDIFF] $in -> $out");
        //ninja.WriteLine ("  description =");
        ninja.WriteLine ("rule emcc");
        ninja.WriteLine ("  command = bash -c '$emcc $emcc_flags $flags -c -o $out $in'");
        ninja.WriteLine ("  description = [EMCC] $in -> $out");
        ninja.WriteLine ("rule emcc-link");
        ninja.WriteLine ($"  command = bash -c '$emcc $emcc_ldflags -o $out_js {jsSources} $in'");
        ninja.WriteLine ("  description = [EMCC-LINK] $in -> $out_js");
        ninja.WriteLine ("rule aot");
        ninja.WriteLine ($"  command = MONO_PATH=in in/mono-aot-cross $process_args --aot=$aot_args,depfile=$depfile $in");

        // Targets
        ninja.WriteLine ("build $builddir/in: mkdir");
        foreach (var infile in infiles)
            ninja.WriteLine ($"build {infile.Value}: cpifdiff {infile.Key}");
        var aotImages = string.Join (" ", AOTAssemblies.Select (item => Path.Combine ("in", Path.GetFileName (item.ItemSpec) + ".o")).ToArray ());
        // AOT
        ninja.WriteLine ($"build in/mono-aot-cross: cpifdiff {AOTCompilerPath}");
        foreach (var item in AOTAssemblies) {
            var file = infiles [item.ItemSpec];
            var processArgs = item.GetMetadata ("ProcessArgs");
            var aotArgs = item.GetMetadata ("AotArgs");
            // Replace the bitcode outfile in the arguments
            var index1 = aotArgs.IndexOf ("llvm-outfile=");
            var index2 = aotArgs.IndexOf (",", index1);
            aotArgs = aotArgs.Substring (0, index1) + $"llvm-outfile={file}.bc.tmp" + (index2 != -1 ? aotArgs.Substring (index2) : "");
            ninja.WriteLine ($"build {file}.bc.tmp: aot {file} | in/mono-aot-cross");
            ninja.WriteLine ($"  process_args={processArgs}");
            ninja.WriteLine ($"  aot_args={aotArgs}");
            ninja.WriteLine ($"  depfile={file}.deps");
            ninja.WriteLine ($"build {file}.bc: cpifdiff {file}.bc.tmp");
            ninja.WriteLine ($"build {file}.o.tmp: emcc {file}.bc");
            ninja.WriteLine ($"build {file}.o: cpifdiff {file}.o.tmp");
        }

        ninja.WriteLine ($"build $builddir/dotnet.js $builddir/dotnet.wasm: emcc-link {ofiles} {aotImages} | {jsDeps}");
        ninja.WriteLine ("  out_js=$builddir/dotnet.js");
        ninja.WriteLine ("  out_wasm=$builddir/dotnet.wasm");

        ninja.Close ();

        return true;
    }
}
