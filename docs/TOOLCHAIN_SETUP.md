# GCC trunk toolchain

The engine is built with a **native-Windows GCC trunk** toolchain (for `-freflection`
+ `import std`). It is distributed as a prebuilt release, exactly like the old
clang-p2996 toolchain — you download it, you don't build it.

## Teammates: install + build

`python bootstrap.py` is the one-shot entry point: it inits submodules, installs the
GCC trunk toolchain, and provisions Ninja into `~/.gcc-trunk/ninja` (the CMake presets
add that to `PATH`, so no global install is needed). To run only the toolchain step:

```
python scripts/install_gcc_trunk.py --persist
cmake --preset x64-mingw-gcc-Release
cmake --build --preset x64-mingw-gcc-Release
```

`install_gcc_trunk.py`:

1. Resolves the latest `gcc-trunk-v*` GitHub release (filters to GCC releases, so a
   coexisting `clang-p2996-*` release is never picked by mistake).
2. Downloads `gcc-trunk-windows-x64.zip` and extracts it to `~/.gcc-trunk/<tag>`.
3. With `--persist`, sets the `MINGW_ROOT` user env var (which the CMake presets read
   as `$env{MINGW_ROOT}/bin/g++.exe`).

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
  (`scripts/gcc-toolchain/recipe/`), then `scripts/package_gcc_toolchain.py` stages
  the relocatable toolchain and zips it, and the zip is published as the release.

> The scheduled trigger only fires for the workflow on the repository's **default
> branch**, so this file must be present on `main` for weekly builds to run.

## Building the toolchain yourself

Same entry point CI uses (needs an MSYS2 install with `base-devel` + the GCC build
deps — see the `install:` list in the workflow):

```
python scripts/build_gcc_trunk.py --snapshot 20260601 --msys-root C:/msys64
```

Produces `dist/gcc-trunk-windows-x64.zip`. The vendored recipe carries MSYS2's
Windows patch stack; a plain `configure && make` of GCC trunk does **not** build on
native Windows. When trunk drifts enough that a vendored patch stops applying, the
weekly build fails loudly — update `scripts/gcc-toolchain/recipe/` and re-dispatch.
