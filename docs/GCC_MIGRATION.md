# GCC Trunk + WSL Migration Plan

Goal: replace the bespoke clang-p2996 + libc++ + MSVC-ABI stack with **GCC trunk inside WSL2 (Ubuntu)** for development, and produce Windows `.exe` artifacts via **MinGW-w64 cross-compilation** (also driven by GCC trunk) for shipping.

This is a one-way migration. If it works, the entire bootstrap pipeline collapses from "build clang fork + build libc++ + build compiler-rt + patch headers + custom triplet + math shim" into "install GCC trunk, configure, build."

---

## Why this works

Reflection (P2996) and `template for` (P1306) landed on GCC trunk in late 2025. Our reflection usage is vanilla P2996 — `^^T`, `[:m:]`, `template for`, `nonstatic_data_members_of`, `define_static_array`, `access_context::unchecked()` — no clang-fork-specific intrinsics. Source code changes are expected to be near-zero (possibly one header swap: `<meta>` → `<experimental/meta>` if GCC hasn't promoted it yet).

The big win is the toolchain: GCC trunk is one upstream repo, one configure, one make. No fork. No header patching. No ABI bridging. No MSVC dependency.

---

## Phase 0: Confirm GCC trunk reflection works (do this first)

Before deleting anything, prove it compiles. Spin up a tiny test in WSL:

```bash
sudo apt update
sudo apt install -y build-essential flex bison libgmp-dev libmpfr-dev libmpc-dev libisl-dev texinfo
git clone --depth=1 git://gcc.gnu.org/git/gcc.git ~/gcc-src
cd ~/gcc-src && ./contrib/download_prerequisites
mkdir ~/gcc-build && cd ~/gcc-build
~/gcc-src/configure --prefix=$HOME/gcc-trunk \
    --enable-languages=c,c++ --disable-bootstrap --disable-multilib
make -j$(nproc) && make install
export PATH=$HOME/gcc-trunk/bin:$PATH
```

Build time: 20–40 min on decent hardware. Disk: ~5 GB build dir, ~1.5 GB install.

Compile a smoke test that exercises `Engine/Engine/Source/Meta/Hash.cppm`'s pattern:

```cpp
#include <experimental/meta>
import std;

struct foo { int a; float b; };

template <typename T>
consteval std::size_t field_count() {
    return std::meta::nonstatic_data_members_of(^^T,
        std::meta::access_context::unchecked()).size();
}

int main() { return field_count<foo>(); }
```

Build: `g++-trunk -std=c++26 -freflection -fcontracts test.cpp` (flag names may differ — check `g++ --help=c++` for the actual names on whatever trunk SHA you grabbed).

If this compiles and runs, proceed. If not, file a GCC bug, stay on clang-p2996, stop reading.

---

## Phase 1: Two `CMakePresets.json` entries

Keep the existing clang-p2996 presets temporarily so you can A/B during migration. Add:

```json
{
  "name": "linux-gcc-trunk-Debug",
  "displayName": "Linux GCC trunk Debug (WSL)",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/out/build/linux-gcc-trunk-Debug",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/Engine/External/vcpkg/scripts/buildsystems/vcpkg.cmake",
    "VCPKG_TARGET_TRIPLET": "x64-linux",
    "CMAKE_C_COMPILER":   "$env{HOME}/gcc-trunk/bin/gcc",
    "CMAKE_CXX_COMPILER": "$env{HOME}/gcc-trunk/bin/g++",
    "CMAKE_CXX_FLAGS_INIT": "-std=c++26 -freflection -mavx2",
    "CMAKE_CXX_MODULE_STD": "ON",
    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
  }
},
{
  "name": "windows-mingw-trunk-Release",
  "displayName": "Windows .exe via MinGW-w64 GCC trunk (cross from WSL)",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/out/build/windows-mingw-trunk-Release",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/mingw-w64-trunk.cmake",
    "VCPKG_TARGET_TRIPLET": "x64-mingw-dynamic",
    "VCPKG_CHAINLOAD_TOOLCHAIN_FILE": "${sourceDir}/cmake/mingw-w64-trunk.cmake",
    "CMAKE_CXX_FLAGS_INIT": "-std=c++26 -freflection -mavx2",
    "CMAKE_CXX_MODULE_STD": "ON"
  }
}
```

Cross toolchain file (new — `cmake/mingw-w64-trunk.cmake`):

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

The MinGW cross-compiler needs to be the same GCC trunk version. Build it via `crosstool-ng`:

```bash
sudo apt install -y autoconf automake libtool libtool-bin gawk help2man texinfo unzip
git clone https://github.com/crosstool-ng/crosstool-ng.git ~/ct-ng-src
cd ~/ct-ng-src && ./bootstrap && ./configure --prefix=$HOME/ct-ng && make && make install
export PATH=$HOME/ct-ng/bin:$PATH
mkdir ~/ct-build && cd ~/ct-build
ct-ng x86_64-w64-mingw32
# edit .config: set CT_GCC_VERSION to "linaro" / custom URL pointing at GCC trunk SHA you used above
ct-ng build
```

Cross toolchain ends up at `~/x-tools/x86_64-w64-mingw32/bin/` — symlink it onto PATH or hardcode that path in `mingw-w64-trunk.cmake`.

---

## Phase 2: Rewrite `CMakeLists.txt` (root)

Replace the entire current 70-line `CMakeLists.txt` with roughly:

```cmake
cmake_minimum_required(VERSION 4.3)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(GSEngine)

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)
add_subdirectory(Server)
```

Everything else in the current root CMake exists only to hold the clang-p2996 + libc++ + MSVC-ABI stack together. It all goes.

### What that deletes from `CMakeLists.txt`

| Removed | Reason it existed |
|---|---|
| `CLANG_P2996_ROOT` env check | Locating the clang fork install |
| `VCToolsInstallDir` / `WindowsSdkDir` / `WindowsSDKVersion` / `UniversalCRTSdkDir` / `UCRTVersion` MSVC env checks | Forcing Developer-PowerShell-only configure for MSVC ABI linking |
| `_gse_libcxx_inc` / `_gse_libcxx_lib` paths and existence checks | Pointing at the custom libc++ build |
| `CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS` override | Using the fork's scan-deps binary |
| `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` UUID gate | CMake's pre-stabilization `import std` opt-in flag |
| `-nostdinc++ -Xclang -cxx-isystem` libc++ injection | Routing `#include <vector>` to libc++ instead of MSVC STL |
| `-D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64` | clang on MSVC target doesn't define these; libc++ requires them. **GCC defines them natively.** |
| `-Xclang --dependent-lib=ucrt` (CXX + C) | Embedding `/DEFAULTLIB:ucrt` into every TU for libc++ runtime |
| `-D_STATIC_INLINE_UCRT_FUNCTIONS=0` | Disabling ucrt's `static inline` wrappers that broke libc++ math calls |
| `-freflection-latest` | clang-p2996-specific flag name (GCC uses `-freflection` or no flag once stabilized) |
| Explicit linker line `${_gse_libcxx_lib}` | Forcing libc++.lib into every link |
| `__cmake_libcxx_std` STATIC target + `__CMAKE::CXX23` / `CXX26` aliases | Telling CMake where our hand-built `std.cppm` / `std.compat.cppm` live |

GCC ships `libstdc++.modules.json` that CMake auto-discovers. `import std;` works with `set(CMAKE_CXX_MODULE_STD ON)` and nothing else.

---

## Phase 3: Rewrite `Engine/CMakeLists.txt`

Currently has a clang-p2996 OOM workaround:

```cmake
# Engine/CMakeLists.txt:25-31
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/Runtime/Engine.cpp"
    PROPERTIES COMPILE_OPTIONS "-O1"
)
```

**Delete it.** The OOM was clang-p2996's eager template instantiation pathology with `add_system`. GCC trunk has no such issue. If it does, we'll reintroduce a similar workaround — but assume it doesn't.

Also has `Engine/Modules/MathShims.cpp` referenced (need to find the line that adds it):

```cmake
# whichever file adds Engine/Modules/MathShims.cpp to Engine target
# — DELETE that target_sources entry.
```

`MathShims.cpp` exists only because libc++'s `<__math/...>` headers called bare `hypotf`/`ldexpf` that ucrt didn't export. libstdc++ on Linux/MinGW resolves these through the normal libm symbol path — no shim needed.

Vulkan-Hpp module block stays unchanged. Slang block stays unchanged (but see Phase 5 — Slang availability differs across triplets).

---

## Phase 4: Files to delete outright

After `git rm`:

| Path | Why it goes |
|---|---|
| `scripts/build_clang_p2996.py` | Builds the fork. We no longer build a fork. |
| `scripts/build_libcxx_p2996.py` | Builds libc++ against vcruntime ABI. libstdc++ ships with GCC. |
| `scripts/build_compiler_rt_p2996.py` | Builds ASAN runtime against MSVC ABI. GCC ships its own sanitizers. |
| `scripts/install_clang_p2996.py` | Downloads our prebuilt clang-p2996 release zip. Obsolete. |
| `.github/workflows/build-clang-p2996.yml` | CI that publishes prebuilt fork releases to GitHub Releases. Replace with a CI that builds GCC trunk once a week (or skip — local builds are fine). |
| `vcpkg-overlays/triplets/x64-windows-release.cmake` | Custom triplet that flipped vcpkg deps to release-CRT-only because MSVC's debug CRT broke ASAN. Linux ASAN has no debug-CRT mismatch. |
| `Engine/Modules/MathShims.cpp` | libc++/ucrt `hypotf`/`ldexpf` workaround. Not needed under libstdc++. |
| `docs/TOOLCHAIN_SETUP.md` | 266 lines documenting the clang-p2996 stack. Replaced by this file + a future `GCC_TRUNK_SETUP.md`. |
| `External/clang-p2996/` (untracked but on disk) | The clang fork sources. |
| `dist/clang-p2996/` (untracked but on disk) | The prebuilt clang fork install. |

After deletion: `vcpkg-overlays/` is empty → delete that too.

---

## Phase 5: `vcpkg.json` and dependency reality check

Manifest itself doesn't change — same packages. But the **triplet** changes per preset:

- **Linux dev (`x64-linux`)** — vcpkg ports for glfw3, freetype, msdfgen, miniaudio, stb, concurrentqueue, shader-slang are all well-maintained. Expect zero-friction install.
- **MinGW cross (`x64-mingw-dynamic`)** — port quality is hit-or-miss. The risky ones:
  - **shader-slang** — official prebuilts are MSVC. May need to build Slang from source under MinGW, or find a community port. **This is the most likely failure point.**
  - **glfw3, freetype, msdfgen, libpng, miniaudio, stb, concurrentqueue** — generally OK under mingw triplets.
  - **vulkan-headers / vulkan-loader** — fine; the loader is just a DLL stub finder.

If Slang is unbuildable under MinGW, fallback options in order:
1. Use the prebuilt MSVC Slang DLL — MinGW can call MSVC C-ABI DLLs fine via dlltool/import-lib generation.
2. Build Slang from source against MinGW (CMake project, may need patches).
3. Wrap the shader compiler in a separate process invoked by the engine (slangc.exe), making toolchain mismatch irrelevant.

---

## Phase 6: `bootstrap.py` rewrite

Current file is 97 lines orchestrating clang-p2996 + libc++ + compiler-rt + vcpkg. Replace with ~20 lines:

```python
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent

def run(cmd, cwd=None):
    print(f"$ {' '.join(str(c) for c in cmd)}")
    if subprocess.run(cmd, cwd=cwd).returncode != 0:
        raise SystemExit("Command failed")

def main():
    if (REPO_ROOT / ".gitmodules").exists():
        run(["git", "submodule", "update", "--init", "--recursive"], cwd=REPO_ROOT)
    if not (Path.home() / "gcc-trunk" / "bin" / "g++").exists():
        print("warning: ~/gcc-trunk/bin/g++ not found.")
        print("Build GCC trunk per docs/GCC_MIGRATION.md Phase 0.")
    print("\nBootstrap complete. Configure with the linux-gcc-trunk-Debug preset.")

if __name__ == "__main__":
    main()
```

---

## Phase 7: `.clangd` update

IDE indexer flags:

```yaml
# Before (Windows MSVC target, clang-p2996)
CompileFlags:
  Add:
    - -std=c++2c
    - -freflection-latest
    - -stdlib=libc++
    - --target=x86_64-pc-windows-msvc
    - -mavx2

# After (Linux GCC libstdc++)
CompileFlags:
  Add:
    - -std=c++26
    - -freflection
    - -mavx2
  CompilationDatabase: out/build/linux-gcc-trunk-Debug
```

clangd indexes fine against libstdc++ headers — it's a clang-tooling indexer, not the clang compiler driver.

---

## Phase 8: `.clang-tidy` — likely unchanged

The checks and naming rules are toolchain-independent. Leave it alone.

---

## Phase 9: Source-code changes (expected to be near-zero)

Files that use reflection (per grep on `^^|std::meta|template for|\[:`):
- `Engine/Engine/Source/Core/ID.cppm` (heavy)
- `Engine/Engine/Source/Toml/Toml.cppm`
- `Engine/Engine/Source/Network/Message/Message.cppm`
- `Engine/Engine/Source/Math/Units/Quantity.cppm`
- `Engine/Engine/Source/Ecs/SystemNode.cppm`
- `Engine/Engine/Source/Containers/Archive.cppm`
- `Engine/Engine/Source/Gpu/Shader/Shader.cppm`
- `Engine/Engine/Source/Meta/Hash.cppm`
- `Engine/Engine/Source/Meta/VariantMatch.cppm` (light)

Likely deltas:
- `#include <meta>` → may need `#include <experimental/meta>` if GCC hasn't promoted the header yet. Maybe a one-line `#if __has_include(<meta>)` shim.
- One or two intrinsic name differences in `std::meta::*` if GCC implemented an older P2996 revision.
- Nothing in the splicer / `template for` / `^^` syntax should change — that's standardized.

If GCC trunk gates reflection behind `<experimental/reflect>` or similar, add a tiny `Engine/Engine/Source/Meta/Reflection.cppm` partition that does the include shim and re-exports. Keeps the rest of the codebase clean.

---

## Phase 10: Memory file update

Update `~/.claude/projects/.../memory/project_build_system.md` to drop "VS 2026 Insiders" and add "GCC trunk via WSL2 / MinGW-w64 cross for Windows release."

---

## Total line count delta (estimated)

| File | Lines before | Lines after | Delta |
|---|---:|---:|---:|
| `CMakeLists.txt` | 69 | ~10 | −59 |
| `Engine/CMakeLists.txt` | 109 | ~95 | −14 |
| `bootstrap.py` | 97 | ~20 | −77 |
| `docs/TOOLCHAIN_SETUP.md` | 266 | 0 | −266 |
| `scripts/build_clang_p2996.py` | ~400 (est) | 0 | −400 |
| `scripts/build_libcxx_p2996.py` | ~200 (est) | 0 | −200 |
| `scripts/build_compiler_rt_p2996.py` | ~200 (est) | 0 | −200 |
| `scripts/install_clang_p2996.py` | ~150 (est) | 0 | −150 |
| `.github/workflows/build-clang-p2996.yml` | 97 | 0 | −97 |
| `vcpkg-overlays/triplets/x64-windows-release.cmake` | 4 | 0 | −4 |
| `Engine/Modules/MathShims.cpp` | 12 | 0 | −12 |
| **Total** | | | **~ −1500 lines** |

Plus we stop maintaining a clang fork build pipeline.

---

## Risk register (in order of likelihood of biting us)

1. **GCC trunk reflection bugs.** Trunk moves daily. Pinning to a known-good SHA is essential.
2. **Slang under MinGW.** See Phase 5 fallbacks. Worst case: ship as separate `slangc.exe` invocation.
3. **MinGW vcpkg ports of less-common deps** (msdfgen with `extensions` feature in particular). Mitigation: try Linux-only first, defer the cross-compile work until the dev loop is solid.
4. **CMake `import std` with libstdc++ + Ninja generator** — works on recent CMake (4.x) but if it doesn't, the fallback is a small handwritten module wrapper, much smaller than the current libc++ shim.
5. **`__has_include(<meta>)` not yet present in GCC trunk** — need a tiny preprocessor shim across ~9 files.

---

## Migration sequence (do in this order, don't reorder)

1. **Phase 0** — get GCC trunk built in WSL, confirm reflection smoke test passes.
2. Build `crosstool-ng` MinGW cross-compiler (Phase 1, tail end). Optional — can be deferred until you actually want a Windows `.exe`.
3. **Add** the new presets and toolchain file (Phase 1) — don't touch the existing clang ones yet.
4. Try to configure the `linux-gcc-trunk-Debug` preset against the *current* `CMakeLists.txt`. It will fail loudly (MSVC env var checks). Fix only the `CMakeLists.txt` checks needed to make it past configure — don't delete the clang stack yet.
5. Iterate on compile errors in the reflection-using files. Fix Phase 9 issues.
6. Once a `linux-gcc-trunk-Debug` build runs the engine successfully, **then** delete the clang stack (Phases 2/3/4/6/7).
7. Update memory and remove `docs/TOOLCHAIN_SETUP.md` last.
8. Tackle MinGW cross (Phase 5) when ready to ship a `.exe`.

The reason for this order: at step 6 you have a working escape route. Until then, the existing clang setup keeps working.
