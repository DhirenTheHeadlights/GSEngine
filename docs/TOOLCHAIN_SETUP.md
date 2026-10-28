# GCC trunk toolchain

The engine is built with a **native-Windows GCC trunk** toolchain (for `-freflection`
+ `import std`). It is distributed as a prebuilt release — you download it, you
don't build it.

## Teammates: install + build

`python bootstrap.py` is the one-shot entry point: it inits submodules, installs the
GCC trunk toolchain, and provisions Ninja into `~/.gcc-trunk/ninja` (the CMake presets
add that to `PATH`, so no global install is needed). To run only the toolchain step:

```
python Tools/install_gcc_trunk.py --persist
cmake --preset x64-mingw-gcc-Release
cmake --build --preset x64-mingw-gcc-Release
```

`install_gcc_trunk.py`:

1. Resolves the latest `gcc-trunk-v*` GitHub release.
2. Downloads `gcc-trunk-windows-x64.zip` and extracts it to `~/.gcc-trunk/<tag>`.
3. With `--persist`, sets the user env vars; the CMake presets resolve the compiler as
   `$env{USERPROFILE}/.gcc-trunk/current/bin/g++.exe` and prepend that `bin` to PATH.

Pin a specific build with `--tag gcc-trunk-v3`, verify integrity with
`--sha256 <hash>` (printed in each release body), reinstall with `--force`.

No Visual Studio, no MSYS2, no libc++/compiler-rt side-build — the zip is a
self-contained toolchain (gcc + binutils + mingw-w64 runtime + libstdc++ `std`
module).

## How releases are produced

`.github/workflows/build-gcc-trunk.yml` runs weekly (Mon 07:00 UTC) and on manual
dispatch:

- **resolve** (Linux): finds the latest GCC trunk weekly snapshot at
  `gcc.gnu.org/pub/gcc/snapshots/`, computes the next `gcc-trunk-vN` tag, and skips
  if that snapshot was already published.
- **build** (Windows): MSYS2 builds GCC from the snapshot through the vendored recipe
  (`Tools/gcc-toolchain/recipe/`), then `Tools/package_gcc_toolchain.py` stages
  the relocatable toolchain and zips it, and the zip is published as the release.

> The scheduled trigger only fires for the workflow on the repository's **default
> branch**, so this file must be present on `main` for weekly builds to run.

## Installing MSYS2 locally

Only needed if you are building a toolchain rather than downloading one.
`python Tools/install_msys2.py` extracts MSYS2's self-extracting base archive into
`<repo>/.generated/msys64` (gitignored) and installs the same package closure CI uses — it reads
the `install:` list straight out of `.github/workflows/build-gcc-trunk.yml`, so the two
cannot drift. Re-running it just updates packages; `--force` re-extracts.

## Building the toolchain yourself

Same entry point CI uses (needs an MSYS2 install with `base-devel` + the GCC build
deps — see the `install:` list in the workflow):

```
python Tools/build_gcc_trunk.py --snapshot 20260601
```

Produces `.generated/dist/gcc-trunk-windows-x64.zip`. The vendored recipe carries MSYS2's
Windows patch stack; a plain `configure && make` of GCC trunk does **not** build on
native Windows. When trunk drifts enough that a vendored patch stops applying, the
weekly build fails loudly — update `Tools/gcc-toolchain/recipe/` and re-dispatch.

## The experimental wasm32 cross compiler

Separate from everything above, and not part of the distributed zip.
`python Tools/build_gcc_wasm.py` builds a `wasm32-unknown-unknown` GCC into
`<repo>/.generated/.gcc-wasm` from a pinned trunk snapshot plus the out-of-tree WebAssembly
backend patches in `Tools/gcc-toolchain/wasm/` (provenance in the README there): the
driver, `cc1`, `cc1plus`, libgcc and a hosted libstdc++ against a pinned wasi-libc
sysroot, assembled by `llvm-mc` and linked by `wasm-ld` from MSYS2. It then runs its probe
table, and `--probe NAME...` re-runs named probes against the toolchain already built.

It does not go through the vendored `makepkg` recipe and does not touch `/mingw64`, so
it cannot affect the toolchain the engine builds with. See `docs/wasm_pipeline_plan.md`
for what it is for and what it has measured.

The engine itself is built for wasm32 through the `wasm32-wasi-gcc-Release` CMake preset,
whose toolchain file `Engine/cmake/toolchains/wasm32-wasi-gcc.cmake` uses that compiler
straight from `.generated/.gcc-wasm-build`. Build it like any configuration (`gse_build`
with that preset); the result is `.generated/out/build/wasm32-wasi-gcc-Release/Sandbox/Sandbox.wasm`,
which runs the scenario named by the `GSE_WASM_SCENARIO` cache variable (default
`scene_reset_cpu`). The build writes its runners beside it, so no Python is needed:
`node Sandbox.mjs [ARGS]` runs it under node, and `node serve.mjs --open` serves the
browser page (fetching each file the run opens from the repo tree) and opens it in Chrome
or Edge. Both resolve paths and baked resources from the native build named by the
`GSE_WASM_NATIVE_BUILD` cache variable (default `x64-mingw-gcc-Release`), so build that
too. `Tools/run_wasm.py` and `Tools/wasm_parity_gate.py` are convenience wrappers over
those two files.
