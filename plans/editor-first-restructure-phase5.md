# Phase 5 — Engine SDK + packaging

Parent: [editor-first-restructure.md](editor-first-restructure.md). Status: **SDK spike PASSED 2026-07-31.** The gate is cleared; what remains is packaging work, not a research question.

Everything here serves *shipping to someone else*. Daily work stays on engine-source-by-default (phase 3a).

## Spike result

A foreign translation unit compiled against relocated BMIs, linked against relocated libraries, and ran:

```
sdk ok: 6 m
```

Setup: the engine's 395 `.gcm` files and 5 `.a` files were copied out of `out/build/x64-mingw-gcc-RelWithDebInfo` into `%TEMP%/gse-sdk-layout` — a path with no relationship to the repo. A consumer TU in a third directory (`%TEMP%/gse-sdk-consumer`) did `import std; import gse;`, used `gse::vec3<gse::length>` and `std::println`, and produced a 197 KB object, a 589 MB executable, and correct output with the unit formatter intact.

Path A (hand-generated flags) is therefore **viable**, and needed only two direct module mappings — `gse` and `std`. Everything else resolved on its own, for the reason below.

## The finding that constrains the design

**GCC bakes each dependency BMI's path into the importing BMI, and does not re-map transitive imports through the module mapper.** Surfaced by:

```
gse.assert: error: failed to read compiled module: No such file or directory
gse.assert: note: compiled module file is 'Engine/CMakeFiles/Engine.dir/gse.assert.gcm'
```

The mapper said `gse.assert` lived at `<sdk>/Modules/gse.assert.gcm`; GCC used the path stored inside `gse.gcm` instead. A second variant confirmed it by prefixing `$root` onto that baked path rather than onto the mapped one.

The baked path is **relative**, and is resolved against the compiler's working directory:

| Image shape | Consumer cwd | Result |
|---|---|---|
| Flat `Modules/*.gcm`, every module mapped | anywhere | **fails** — transitive deps unresolvable |
| Original `Engine/CMakeFiles/Engine.dir/…` layout preserved | SDK root | **works** — all 395 BMIs load |

So the SDK image is **a relocatable copy of a build tree**, not an arbitrary directory of modules. Two consequences:

- The packager must preserve the module tree's relative layout verbatim. It cannot flatten, rename, or reorganise into a prettier `Modules/` directory.
- The consumer must compile with its root at the SDK. CMake sets the working directory to the *consumer's* build tree, so either the SDK is unpacked into that build tree at the expected relative path, or the generated project passes a root that makes the baked paths resolve. **This is the main open design question for Path B.**

## Gotchas found the hard way

- **`-fmodule-mapper=C:/…` is ambiguous with GCC's `host:port` socket syntax** and fails with no diagnostic at all. CMake always passes a *relative* mapper path; so must the packager and any generated build.
- **Relative entries inside a hand-written mapper did not resolve against cwd** even with `$root .`, while absolute entries did — and baked transitive paths *do* resolve against cwd. That asymmetry was not fully isolated and cost several iterations. Understand it before writing the real packager.
- **`g++` exits non-zero with zero output** when the toolchain's own DLLs are not on `PATH`. Any packaging script that shells out to the compiler must put `<toolchain>/bin` on `PATH` first or it will look like a silent, causeless failure.
- The engine's phase-1 path resolution already behaves correctly for an install image: without `gse.manifest` the app refuses to start and says why, then works once one is supplied. The `mode = installed` hook is real, not theoretical.

## Measured sizes

| | |
|---|---|
| Engine BMIs (395 files) | 200 MB |
| `std` + `std.compat` BMIs | 34 MB |
| `libEngine.a` | 710 MB |
| Linked consumer exe | 589 MB |

Per config. Shipping Debug **and** RelWithDebInfo puts the image comfortably at the top of the parent plan's "1-2 GB incl. toolchain" estimate, before the toolchain's own ~580 MB.

## Remaining work

1. **`install()` / `export()` rules.** There are currently none anywhere — every consumer today uses `add_subdirectory`. This is the bulk of the mechanical effort.
2. **SDK package script**, assembling the image with the module tree's layout preserved. `package_gcc_toolchain.py` is the model, and only covers the compiler.
3. **Vendor the dependencies.** The spike borrowed vcpkg's static libs and runtime DLLs from the build tree; the image needs its own copies plus the gcc runtime DLLs that `gse_copy_runtime_deps` normally supplies.
4. **Toolchain lockstep stamp.** `gse_write_manifest()` writes only `mode` and `root`. It needs the product version and a toolchain hash, and the editor must refuse a mismatch loudly — a BMI/compiler mismatch presents as "Bad file data" on every module, and a plugin mismatch as every analyzed TU failing identically. Neither names the real cause.
5. **`[engine] version = <n>`** resolution in `project::load()`, alongside the existing `name` and `source`.
6. **Per-config trees** — `Modules/<config>`, `Lib/<config>`.
7. **`std.gcm` policy.** `CMAKE_CXX_MODULE_STD ON` builds it per build tree today. Shipping it means matching flags exactly; not shipping it costs every consumer one build. Sharp edge #4 already parks this as a later optimisation.
8. **Inno Setup installer**, per the parent plan.

## Path B

`install(TARGETS … FILE_SET CXX_MODULES)` + `find_package` remains untried. It is the newest corner of CMake's module support and GCC is the least-exercised compiler in it, so it should be attempted *after* the packager works via Path A rather than bet on. The baked-path constraint above applies to it identically — whatever CMake generates still has to make those relative paths resolve.
