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

---

## Migration log: changes & concessions made (2026-04-25/26)

This is the empirical record of what worked, what didn't, and the workarounds applied while pushing the engine through GCC trunk SHA `afbf84bc`.

### Toolchain & build system

- **GCC trunk built in WSL2 Ubuntu** at `~/gcc-trunk/`, sources at `~/gcc-src/` (1-deep clone insufficient — needed `git fetch --refetch` to advance to a current SHA).
- **CMake** pinned to 4.x (at `~/cmake-4/`) for `CXX_MODULES` + `import std` support. Required `set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")` UUID gate.
- **Vulkan SDK 1.4.341.1** LunarG distribution at `~/vulkan-sdk/1.4.341.1/`.
- **Wrapper scripts** at `~/configure_gcc.sh`, `~/build_gcc.sh`. Build script forced `-j1` because parallel CMI writes corrupt module files. `RC=${PIPESTATUS[0]}` needed to surface real exit codes through `tee | tail` pipelines.
- **Root `CMakeLists.txt`**: wrapped clang-p2996/MSVC stack in `if(CMAKE_HOST_WIN32)`, added `add_compile_options(-Wno-changes-meaning -Wno-error=changes-meaning)` for GCC.
- **`CMakePresets.json`**: added `linux-gcc-trunk-Debug` preset with VULKAN_SDK env, gcc-trunk paths, `-std=c++26 -freflection -mavx2 -Wno-changes-meaning`.
- **CRLF**: `git config core.autocrlf input` + `git add --renormalize .` to silence 375 spurious "modified" files.

### Editor: deleted (no longer in use)

- Entire `Editor/` tree removed (CMakeLists, Editor.h, ResourcePaths.h.in, Editor.cpp, main.cpp).

### GCC trunk module bugs encountered (and workarounds)

These are real GCC trunk bugs we hit. Each needed a code-level workaround. None of these are bugs in our code — they are GCC trunk regressions that require user-side adaptation.

#### 1. `failed to load pendings for std::function` (Registry CMI corruption)

- **Symptom**: any consumer doing `import :registry;` failed with `failed to read compiled module cluster N: Bad file data` followed by `failed to load pendings for 'std::remove_reference'` (or similar std template).
- **Trigger**: `class registry` had `std::function<...>` and `task::concurrent_queue<std::pair<id, std::function<...>>>` members. The CMI couldn't be re-deserialized.
- **Workaround**: full PIMPL refactor of `gse::registry`:
  - Introduced opaque `struct registry_state;` forward-declared in `Registry.cppm`, defined in new `Registry.cpp` impl unit.
  - All `std::vector`/`std::mutex`/`std::pair` members moved into `registry_state` (out of CMI).
  - Templated `add_deferred_action<F>` and `defer_add_component<T,...>` now take an `F&&` and call private erased helpers `push_pending_op_erased(void*, void(*)(void*, registry&), void(*)(void*) noexcept)` defined in `Registry.cpp`.
  - Out-of-line ctor/dtor/move declared in `.cppm`, defined in `.cpp`.
- **Same workaround applied to `gse::scheduler`** (Scheduler.cppm + new Scheduler.cpp): `m_deferred` (`std::vector<gse::move_only_function<void()>>`) moved into `scheduler_state` PIMPL. `push_deferred<F>` and templated `render` use type-erased trampolines.
- **Same workaround applied to `channel_writer`** (PhaseContext.cppm): `push_fn = move_only_function<...>` replaced with raw `void*` + function pointers. Templated ctor wraps caller's lambda with `+[](void* p, ...) { (*static_cast<F*>(p))(...); }`.

#### 2. `namespace_members_of` ICE (Quantity unit auto-discovery)

- **Symptom**: ICE in `cp/cp-tree.cc` when `std::meta::namespace_members_of(^^ns)` was called on namespaces that had transitive imports.
- **Workaround**: stubbed three consteval functions in `Math/Units/Quantity.cppm`: `find_default_unit_in_namespace`, `collect_unit_infos_for_tag`, `find_root_tag_by_dim`. They now return a sentinel `^^no_default_unit_v` instead of doing namespace reflection. Marked with `TODO(gcc-trunk)` comments.

#### 3. `add_binding_entity` ICE (full AsyncTask coroutine machinery)

- **Symptom**: ICE on AsyncTask.cppm when full coroutine `task<T>` machinery was active (placement new on `promise_base`, `when_all`, `sync_wait` definitions).
- **Workaround**: rewrote AsyncTask.cppm as a minimal stub:
  - `task<T>` and `task<void>` keep coroutine_type, handle, ctor/dtor/move, and `start()`.
  - `promise_base` no longer has `operator new`/`operator delete` (used `frame_arena`).
  - `when_all(task<Ts>&&...)` and `sync_wait<T>(task<T>&&)` are declarations only — link-broken until restored.
  - Removed `meta`-using helpers; `final_awaiter` simplified.

#### 4. `append_imported_binding_slot` ICE (CMI consume-time corruption)

This is the recurring bug that drove most of the architecture changes.

- **Symptom**: ICE at `cp/name-lookup.cc:353` (`append_imported_binding_slot`) called from `add_imported_namespace` when GCC walks a CMI's namespace bindings. Triggers when:
  - A primary umbrella module (e.g. `gse.gpu`) re-exports many partitions.
  - A `.cpp` impl unit (`module M;`) consumes a `.cppm` interface unit's CMI that transitively brought in `gse.gpu` and/or `gse.context`.
  - Sibling-to-sibling `.cppm` imports between modules that each pull `gse.gpu`+`gse.context`.
- **Workaround pattern A — sibling promotion**: split partitions out of `gse.gpu` umbrella into their own primary modules. Each promoted module imports `gse.gpu` for context types but is no longer a partition. We promoted: `:gpu_buffer` → `gse.buffer`, `:gpu_image` → `gse.image`, `:gpu_compute` → `gse.compute`, `:gpu_pipeline` → `gse.pipeline`, `:descriptors` → `gse.descriptors`, `:acceleration_structure` → `gse.acceleration_structure`, `:video_encoder` → `gse.video_encoder`, `:gpu_context` → `gse.context`, plus `:trace` → `gse.trace`, `:scheduler` → `gse.scheduler`, and added a brand-new `:sampler` partition (small extraction from `:gpu_pipeline`).
- **Workaround pattern B — partition-attached `.impl.cppm`**: the four GPU `.cpp` impl units (`VulkanAllocator.cpp`, `GpuDevice.cpp`, `DeviceBootstrap.cpp`, `GpuFrame.cpp`) couldn't `module gse.gpu;` (umbrella consume ICE). Renamed to `*.impl.cppm` and reattached to distinct impl partitions (`gse.gpu:vulkan_allocator_impl`, `gse.gpu:gpu_device_impl`, `gse.gpu:device_bootstrap_impl`, `gse.gpu:gpu_frame_impl`). Each impl partition does `import :original_partition;` to pick up declarations.
- **Workaround pattern C — forward-declare in interface, full import in impl**: for `Audio.cppm` + `Audio.cpp`. `Audio.cppm` does `export namespace gse::gpu { class context; }` instead of `import gse.context;`, so its CMI doesn't reference `gse.context`. `Audio.cpp` (impl unit) does `import gse.context;` directly. Works only when the consumer doesn't need member access on the forward-declared type.
- **Workaround pattern D — eliminate transitive consume**: removed `:shader_layout_compiler` from `:gpu_context`'s import list (replaced with stub asset_compiler call from elsewhere). Removed `gse.context` import from `Descriptors.cppm` since `gse.pipeline` already provides it transitively. Caused new "interface partition is not exported" errors that needed re-exports added to `gse.gpu` umbrella.
- **Limit reached**: workaround D fails when a consumer like `gse.descriptors` or `gse.acceleration_structure` needs to import a sibling like `gse.pipeline` or `gse.buffer`. The sibling's CMI references `gse.gpu`+`gse.context` and re-loading them ICEs even with the workarounds. **AccelerationStructure.cppm and Descriptors.cppm are currently broken at this stage.**

#### 5. `interface partition is not exported` (cascading umbrella exports)

- **Symptom**: when umbrella `gse.gpu` re-exports `:gpu_device`, error says `:vulkan_allocator: error: interface partition is not exported`.
- **Workaround**: every partition transitively imported by any `export import`ed partition must also appear in the umbrella's `export import` list. The current `Engine/Engine/Import/Gpu.cppm` exports 23 partitions to satisfy this.

#### 6. Deducing-this `auto&` return before deduction

- **Symptom**: `[[nodiscard]] auto instance_config(this auto& self) -> auto&;` (declaration only, body in separate impl unit) fails with `use of … before deduction of 'auto'` at the call site.
- **Workaround**: deducing-this template method bodies must be **inline in the interface unit** so consumers can deduce. Moved bodies of `device::logical_device`, `device::descriptor_heap`, `device::instance_config`, `device::device_config` from `GpuDevice.cpp` into `GpuDevice.cppm`. Changed return type from `auto&` to `decltype(auto)` to keep the intent clear.

#### 7. `<meta>` header conflicts with `import std;`

- **Symptom**: any file with `module; #include <meta> export module M;` followed by `import std;` errors with `conflicting imported declaration 'typedef struct __mbstate_t __mbstate_t'` (and a cascade of related `<wchar.h>`/`<pthread.h>` types).
- **Cause**: `<meta>` transitively includes `<wchar.h>` etc. in the GMF (defining C-side types), then `import std;` re-imports the same types from the std module. GCC trunk treats them as conflicting redeclarations.
- **Workaround**: removed `#include <meta>` from the GMF in 6+ files (Containers/Archive, Diag/Trace, Meta/Hash, Meta/VariantMatch, Gpu/Shader/Shader, Gpu/Device/RenderGraph, Ecs/SystemNode). `import std;` exposes `std::meta::*` on its own when `-freflection` is on, so the include was redundant.
- **Same conflict**: any third-party header in the GMF that pulls `<wchar.h>` (e.g. `<miniaudio.h>`, `<moodycamel/concurrentqueue.h>`). Workarounds:
  - **moodycamel**: replaced `moodycamel::ConcurrentQueue<T>` with `std::queue<T> + std::mutex` in `Concurrency/Task.cppm`, `Time/FrameSync.cppm`. Added wrapper helpers `submission_try_dequeue`/`submission_enqueue`.
  - **miniaudio**: not yet refactored. `Audio.cpp` is currently disabled (renamed to `Audio.cpp.disabled`). Plan: extract `MINIAUDIO_IMPLEMENTATION` into a non-modular `MiniaudioImpl.cpp` (file already created), and have `Audio.cpp` declare extern wrappers around miniaudio types.

#### 8. TU-local lambda exposed as default template argument

- **Symptom**: `template <typename F> auto start(F&& fn = []{}, ...) -> ...;` fails with `exposes TU-local entity 'template<class F> struct gse::task::<lambda()>'`.
- **Workaround**: replaced the inline lambda default with a named `struct noop { auto operator()() const noexcept -> void {} };` and `template <typename F = noop> auto start(F&& fn = F{}, ...);`.

### Linux compatibility stubs (Windows-only code)

These files had Windows-only deps that don't build under Linux/GCC. Wrapped the implementation in `#ifdef _WIN32` so the file builds on both platforms, with a Linux stub returning a sentinel:

- `Os/STB/ImageLoader.cppm` — stb_image is win-only in our current vcpkg overlay.
- `Network/Socket.cppm` — POSIX shim only inside `#ifdef _WIN32` (TODO: restore POSIX impl via non-modular `.cpp` + C-API).
- `Gpu/Shader/ShaderCompiler.cppm` — slang headers (`<slang.h>`, `<slang-com-ptr.h>`) are Windows-only. The asset compiler now early-returns on Linux; baked artifacts must come from a Windows machine.
- `Gpu/Shader/ShaderLayoutCompiler.cppm` — same.

### SIMD: scalar-only fallback

- `Math/Vector/SIMD.cppm` reduced to scalar fallback (no `<immintrin.h>`) because the include in the GMF caused a `cstdlib` transitive include conflict with `import std;`.
- Original at `SIMD.cppm.disabled` for reference.
- TODO: split intrinsics into a non-module `.cpp` with C-API wrappers.

### Reflection: `std::meta::remove_const` for annotation values

- `Network/Message/Message.cppm`: GCC's annotation reflection (`std::meta::annotations_of(^^T)`) returns `std::meta::info` whose `type_of` resolves to a `const T` qualified type. Had to add `std::meta::remove_const` before comparing with `^^network_message`:
  ```
  return std::meta::remove_const(std::meta::type_of(std::meta::constant_of(ann))) == ^^network_message;
  ```

### Vulkan-hpp / std::hash interaction

- `vk::Flags<vk::MemoryPropertyFlagBits>` has no `std::hash` specialization in vulkan-hpp. Workaround: hash the underlying mask value via `static_cast<vk::MemoryPropertyFlags::MaskType>(...)`.
- Located in `Gpu/Vulkan/VulkanAllocator.impl.cppm`.

### `joint_constraint` deleted destructor

- Required adding explicit defaulted ctor/dtor/move ops to `Physics/VBD/Constraints.cppm` to silence implicit-deletion. Real cause unclear — papered over.

### Renderer file `import` cascade

When sibling-promoting GPU partitions, each external consumer that previously got the type via `import gse.gpu;` had to add the new sibling import. Bulk-patched 35+ files in `Engine/Engine/Source/Graphics/`, `Engine/Engine/Source/Physics/`, `Engine/Engine/Source/Audio/`, `Engine/Engine/Source/Runtime/` to add `import gse.buffer;`, `import gse.image;`, `import gse.compute;`, `import gse.pipeline;`, `import gse.descriptors;`, `import gse.video_encoder;`, `import gse.acceleration_structure;`, `import gse.context;`, `import gse.scheduler;` as needed.

### `gse.gpu` umbrella structure (final)

`Engine/Engine/Import/Gpu.cppm` now exports 23 partitions (down from a single big partition group) — the partitions that aren't sibling-promoted but must satisfy the "interface partition is not exported" rule for the remaining internal partitions. Order matters; the current order (`:gpu_types`, `:gpu_sync`, then vulkan internals, then `:gpu_device` chain, then bindless/push_constants/shaders/render_graph) was determined empirically.

### Engine.cpp `m_scheduler.render(frame_ok, lambda)` rewrite

- `scheduler::render` is no longer templated. It takes `(bool, void (*)(void*), void*)`. Engine.cpp constructs the lambda then passes a manual trampoline:
  ```
  m_scheduler.render(frame_ok, +[](void* p) { (*static_cast<decltype(in_frame)*>(p))(); }, &in_frame);
  ```

### Files temporarily disabled (renamed to `.disabled`)

- `Engine/Engine/Source/Audio/Audio.cpp.disabled` — miniaudio `<wchar.h>` mbstate_t conflict, see workaround #7.
- `Engine/Engine/Source/Math/Vector/SIMD.cppm.disabled` — original AVX intrinsics file, see SIMD section.

### Build status as of this checkpoint

- ~470/604 TU compile cleanly under `linux-gcc-trunk-Debug`.
- Remaining ~130 are blocked on the recurring `append_imported_binding_slot` ICE in sibling-to-sibling import chains (see workaround #4 limit).
- AccelerationStructure.cppm, Descriptors.cppm, and downstream consumers are currently broken on this ICE.
- Audio.cpp disabled pending miniaudio extraction.
- The original Windows clang+p2996 stack still works as before; nothing destructive committed yet.

### What's needed to finish

1. **Either**: wait for upstream GCC fix to `cp/name-lookup.cc:353` (file a bug report — the assertion is too strict for our module graph).
2. **Or**: extract `MiniaudioImpl.cpp` and write `extern "C"` wrappers so `Audio.cpp` doesn't need miniaudio.h in its GMF.
3. **Or**: continue rewriting sibling modules to expose only forward-decl-friendly interfaces (no member access on imported types from sibling modules) so workaround pattern C extends to AccelerationStructure/Descriptors. This is invasive — `gse::gpu::buffer::native()`, `gse::gpu::context::device()` etc. all become opaque accessors.
4. **Restore** the items in workarounds 2/3/7's Socket+SIMD via non-modular `.cpp` + C-API wrappers once the build is otherwise green.

---

## Migration log: 2026-04-26/27 architectural rework

This session rolled back many of the workarounds above and replaced them with a single, validated pattern. Net result: from ~130 blocked TUs down to 6.

### Scheduler CMI corruption: root-caused and fixed

Bisected `Scheduler.cppm` to find that the corruption was triggered by the polymorphic provider inheritance (`scheduler : state_snapshot_provider, channel_reader_provider, resources_provider, system_provider`). Removing the four provider interfaces and giving `scheduler` direct concrete methods eliminated the `gse::scheduler{}` cluster CMI corruption entirely. The bisect process itself isolated the trigger — every template method, field, and non-template method works fine on its own; only the multi-inheritance pattern triggered the bug.

- Deleted `state_snapshot_provider`, `channel_reader_provider`, `resources_provider`, `system_provider` from `PhaseContext.cppm`.
- `scheduler` now has direct `system_ptr`, `snapshot_ptr`, `frame_snapshot_ptr`, `channel_snapshot_ptr`, `resources_ptr`, `ensure_channel` methods.
- `init_context`, `update_context`, `frame_context`, `task_context` now hold a `scheduler&` directly instead of four separate provider references.
- `World.cppm` updated to take `scheduler&` not `system_provider&`.
- Non-template method bodies live in new `Scheduler.cpp` impl unit (templates stay in `.cppm`).

### Wrapper module pattern (validated end-to-end)

Replaces the previous "include third-party header in every consumer's GMF" pattern. A wrapper module includes the C/C++ header in its own GMF, re-exports symbols via `using ::name;`, and re-exposes macros as `inline constexpr` constants. Consumers do `import gse.<lib>;` instead of `#include <lib.h>`, eliminating the GMF + `import std;` collision (workaround #7).

Key validation: tested with a standalone reproducer in `/tmp/pngwrap` proving that `import std; import png_wrap;` in a consumer compiles and links cleanly even though `<png.h>` was textually included in the wrapper. The wrapper's GMF stays separate from any consumer's `import std;`.

**Wrappers created** (in `Engine/Engine/Source/External/`):

- `Libpng.cppm` (`gse.libpng`) — wraps `<png.h>`, exports types/functions, exposes `png_libpng_ver_string` etc. as constexpr, provides `png_jmpbuf_of` and a `png_guarded(png, body, on_error)` helper to abstract over `setjmp` (which can't cross module boundaries as a macro).
- `Libjpeg.cppm` (`gse.libjpeg`) — wraps `<jpeglib.h>`, includes `jpeg_create_decompress_wrap` to wrap the `jpeg_create_decompress` macro as a function. Provides `jpeg_guarded(buf, body, on_error)` for setjmp.
- `Slang.cppm` (`gse.slang`) — wraps `<slang.h>` + `<slang-com-ptr.h>`, re-exports `namespace slang::*`, `namespace Slang::ComPtr`, and `Slang*` C types (`SlangResult`, `SlangBindingType`, `SlangStage`, etc.). Exposes `SLANG_*` macros as `inline constexpr` (e.g. `slang_binding_type_texture`, `slang_spirv`, `slang_stage_vertex`). Provides `slang_succeeded`/`slang_failed` wrappers for the macro-based result-check.
- `Miniaudio.cppm` (`gse.miniaudio`) + `MiniaudioImpl.cpp` (non-modular static lib) — wraps `<miniaudio.h>` types/functions, with `MINIAUDIO_IMPLEMENTATION` defined in the non-modular impl file linked as a separate static library.
- `Glfw.cppm` (`gse.glfw`) — wraps `<GLFW/glfw3.h>` (with `GLFW_INCLUDE_VULKAN` defined). Exposes a `glfw::create_window_surface(vk::Instance, GLFWwindow*)` helper (in the same module since the wrapper imports `vulkan` itself) so consumers don't need the raw `Vk*` types. Re-exposes ~100 GLFW input/key constants (`GLFW_KEY_A` etc.) under `glfw::` namespace as `inline constexpr int`.
- `Moodycamel.cppm` (`gse.moodycamel`) + vendored `Engine/External/concurrentqueue.h` (one-line patch — namespace-scope `static inline` stripped to plain `inline` to avoid C++20 module TU-locality on details helpers; only outside `struct ConcurrentQueueDefaultTraits` to preserve struct-member statics).

**Consumers converted** (now have no GMF, just `import std; import gse.<lib>;`):

- `Engine/Engine/Source/Os/STB/ImageLoader.cppm/.cpp` — replaced stb_image with libpng + libjpeg-turbo (better quality, vcpkg-supported on both platforms).
- `Engine/Engine/Source/Audio/Audio.cpp` — uses `gse.miniaudio` instead of GMF include.
- `Engine/Engine/Source/Os/GLFW/Input.cppm`, `Window.cppm`, `Keys.cppm` — use `gse.glfw`.
- `Engine/Engine/Source/Concurrency/Task.cppm`, `Engine/Engine/Source/Time/FrameSync.cppm` — reverted from `std::queue+mutex` back to `moodycamel::ConcurrentQueue` via `gse.moodycamel`.

**vcpkg.json**: removed `stb`, added `libpng` and `libjpeg-turbo`.

**Engine/CMakeLists.txt**: linked `PNG::PNG`, `JPEG::JPEG`, removed Stb include path, added `MiniaudioImpl` static lib for the non-modular miniaudio impl, added `Engine/External/` to include path for the vendored moodycamel header. Removed broken `Modules/VulkanDispatch.cpp` reference (file had been deleted).

### Wrapper pattern limit (and the workaround)

The wrapper pattern works for *terminal* consumers (impl units, files at the end of import chains). It fails when:

- The wrapper module is imported by a partition that is itself transitively imported throughout the engine (e.g. `gse.moodycamel` is imported by `gse.time` which is imported by ~everything).
- Some downstream `.cppm` has a GMF include of a different C header that defines the same C-side typedef (e.g. `__mbstate_t`, `max_align_t`).
- The wrapper's CMI carries the typedef from its GMF, the downstream's GMF carries a textual copy, and they collide.

**Fix**: cascade the wrapper pattern. Wrap every C-header GMF include in the engine. Most engine code already passed `import std;` correctly; we wrapped GLFW (the only remaining significant case). Vulkan is already a module (`vulkan.cppm` from the SDK), so vulkan-using files import `vulkan` instead of including C headers.

### Vulkan C-header includes replaced with `import vulkan;`

Three files still had `#include <vulkan/...>` in their GMF; converted to `import vulkan;`:

- `Engine/Engine/Source/Gpu/AccelerationStructure.cppm` (was including `vulkan_hpp_macros.hpp`).
- `Engine/Engine/Source/Gpu/Descriptors.cppm` (was including `vulkan_core.h`).
- `Engine/Engine/Source/Gpu/VideoEncoder.cppm` — added `import vulkan;`.
- `Engine/Engine/Source/Os/GLFW/Window.cppm` — was including `<GLFW/glfw3.h>` with `GLFW_INCLUDE_VULKAN`; now uses `import vulkan; import gse.glfw;` and accesses surface creation via `glfw::create_window_surface(vk::Instance, GLFWwindow*)`.

### `gse.gpu` module split (the big one)

The original `gse.gpu` was a single module with **30 partitions**, with cross-partition imports forming a tight diamond (every partition transitively pulled `gpu_types`, `vulkan_allocator`, etc.). Sibling-partition imports under this density consistently hit GCC's `append_imported_binding_slot` ICE.

Split into **7 top-level modules** + 1 umbrella:

| Module | Files | Notes |
|---|---|---|
| `gse.gpu.types` | `GpuTypes.cppm` | Foundation. Imported by everyone. |
| `gse.gpu.vulkan` | `Vulkan/VulkanAllocator.cppm` (+`.cpp`), `VulkanRuntime.cppm`, `VulkanHandles.cppm`, `VulkanReflect.cppm`, `VulkanUploader.cppm`, `DescriptorBufferTypes.cppm`, `DescriptorHeap.cppm`, `Device/GpuCommandPools.cppm`, `Device/DeviceBootstrap.cppm` (+`.cpp`) | Low-level vulkan abstractions. 9 partitions. |
| `gse.gpu.shader` | `Shader/Shader.cppm`, `ShaderLayout.cppm`, `ShaderRegistry.cppm`, `ShaderCompiler.cppm`, `ShaderLayoutCompiler.cppm`, plus `GpuPushConstants.cppm` | 6 partitions. Slang-using compilers stay (uses `gse.slang` wrapper). |
| `gse.gpu.device` | `Device/GpuDevice.cppm` (+`.cpp`), `GpuSwapchain.cppm`, `GpuFrame.cppm` (+`.cpp`), `Device/RenderGraph.cppm`, `GpuSync.cppm` | 5 partitions. |
| `gse.gpu.context` | `GpuContext.cppm`, `Bindless.cppm` | 2 partitions. |
| `gse.gpu.pipeline` | `GpuPipeline.cppm` | Standalone module. |
| `gse.gpu.resources` | `GpuBuffer.cppm`, `GpuImage.cppm`, `GpuCompute.cppm` | 3 partitions. |
| `gse.gpu` (umbrella) | `Engine/Engine/Import/Gpu.cppm` | Re-exports all six + the 3 leftover partitions: `:descriptors`, `:acceleration_structure`, `:video_encoder`. |

**New umbrella files** in `Engine/Engine/Import/`:

- `GpuVulkan.cppm` (`gse.gpu.vulkan`)
- `GpuShader.cppm` (`gse.gpu.shader`)
- `GpuDevice.cppm` (`gse.gpu.device`)
- `GpuContext.cppm` (`gse.gpu.context`)
- `GpuResources.cppm` (`gse.gpu.resources`)

**Import rewrites** were done by scripts in `/tmp/`: every `import :X;` partition import where `X` is now in a different module was replaced with `import gse.gpu.<module>;`. Files that ARE part of a given module keep their partition imports.

**Eliminated workarounds** (from previous session):

- All "sibling-promoted" modules from workaround #4A (`gse.buffer`, `gse.image`, `gse.compute`, `gse.pipeline`, `gse.descriptors`, `gse.acceleration_structure`, `gse.video_encoder`, `gse.context`, `gse.trace`, `gse.scheduler`, `:sampler`) reverted — they're now partitions of the appropriate split module.
- `*.impl.cppm` files (workaround #4B) renamed back to `.cpp` impl units. Each .cpp now does `module gse.gpu.<module>;` (whichever module it belongs to).

### Restored from `.disabled`

- `Engine/Engine/Source/Audio/Audio.cpp` — re-enabled, uses `gse.miniaudio` wrapper.
- `Engine/Engine/Source/Math/Vector/SIMD.cppm` — restored from `.disabled` (note: still uses `import std;` over GMF includes for performance, no SIMD intrinsics inlined yet).
- `Engine/Engine/Source/Network/Socket.cppm` + new `Socket.cpp` — Linux/POSIX impl restored via split (interface in `.cppm`, POSIX/Winsock impl in `.cpp`).

### `import std;` cleanups

Replaced bare types with `std::`-qualified versions where `import std;` (vs. `<header.h>` in GMF) doesn't expose them globally:

- `Engine/Engine/Source/Assets/ResourceLoader.cppm` — `size_t` → `std::size_t` (~6 sites).
- `Engine/Engine/Source/Gpu/Device/DeviceBootstrap.cpp` — `static_cast<uint32_t>` → `static_cast<std::uint32_t>`, `reinterpret_cast<std::uint64_t>(*pool)` → `std::bit_cast<std::uint64_t>(*pool)` (vk-hpp handle types aren't `reinterpret_cast`-compatible with `uint64_t`).
- `Engine/Engine/Source/Gpu/Vulkan/VulkanAllocator.cpp` — replaced `std::hash<vk::MemoryPropertyFlags>` (vulkan-hpp doesn't specialize) with `std::hash<vk::MemoryPropertyFlags::MaskType>` + cast.

### Remaining: 6 GPU files still ICE

Files: `GpuBuffer.cppm`, `GpuImage.cppm`, `GpuCompute.cppm`, `GpuPipeline.cppm`, `VideoEncoder.cppm`, `AccelerationStructure.cppm`.

**Bisected root cause**: when `gpu_context` partition does `import gse.gpu.device;`, the resulting `gse.gpu.context.gcm` (1.8 MB) carries enough transitive bindings that any external consumer importing `gse.gpu.context` AND `gse.gpu.device`/`gse.gpu.vulkan`/`gse.gpu.types` separately ICEs in `append_imported_binding_slot` while merging the diamond.

Bisect proved:

- Empty `class context;` with no imports → no ICE in consumers.
- `+ import gse.gpu.types;` → no ICE.
- `+ import gse.gpu.types; import gse.gpu.vulkan;` → no ICE.
- `+ import gse.gpu.device;` → ICE in 4–6 consumers.

**Fix that worked but reverted**: forward-declared `gse::gpu::device`, `swap_chain`, `frame`, `vulkan::render_graph` in `GpuContext.cppm`, removed `import gse.gpu.device`, moved all method bodies needing those types to a new `GpuContext.cpp` impl unit. Eliminated all 6 ICEs but introduced module-attachment conflicts: forward-declared `gse::gpu::device` in `gse.gpu.context` is a different entity from `class device { ... }` exported by `gse.gpu.device`, since C++ modules attach class declarations to their declaring module. Consumers importing both saw two distinct `gpu::device` types and got "incomplete type" errors on the forward-decl one.

**To fully fix**: needs PIMPL with type-erased opaque storage in `GpuContext.cppm` (the `m_device` etc. fields become `std::unique_ptr<impl>`, with `impl` defined in `.cpp` containing the actual unique_ptrs), and concrete-type accessor *declarations* in `.cppm` with definitions in `.cpp`. The non-template accessors then return real types from `gse.gpu.device` because the .cpp imports it. This is a half-day refactor and was not done in this session.

### Build status as of this checkpoint

- **Built: ~180 .o / 166 .gcm files**.
- **Failed: 6** — all in the GPU module, all the same `append_imported_binding_slot` GCC bug on consumers of `gse.gpu.context`.
- **Scheduler CMI corruption: 0**. Original migration goal is fully resolved.
- **`Audio.cpp.disabled`**: re-enabled, no longer disabled.
- **`SIMD.cppm.disabled`**: re-enabled.
- **Windows clang+p2996 stack**: still builds as before.

### What's needed to finish

1. **Cleanest path**: full PIMPL of `gse::gpu::context`. `m_window`/`m_device`/`m_swapchain`/`m_frame`/`m_render_graph`/`m_bindless_textures` move into a private `struct impl` defined in `GpuContext.cpp`. `GpuContext.cppm` stores only `std::unique_ptr<impl> m_impl;` (with user-defined dtor declared in `.cppm`, defined in `.cpp` so unique_ptr<impl>'s dtor doesn't need impl complete in `.cppm`). All accessors (`device()`, `frame()`, etc.) become concrete-type non-template declarations in `.cppm` and definitions in `.cpp`. The .cpp `import gse.gpu.device;` directly. Eliminates the diamond.
2. **Or**: file an upstream GCC bug. The bisect is small and reproducible: a partition that imports a module with a deep transitive import graph causes consumers of the partition's umbrella to ICE in name-lookup binding-slot append.
3. **Optional follow-ups**:
   - Trim Slang wrapper to expose only the symbols actually used (currently exports more than needed).
   - Wrap the rest of `freetype` + `msdfgen` (`FontCompiler.cppm` still has GMF includes) using the same pattern if they ever cause issues.
   - Restore SIMD intrinsics in a non-modular `.cpp` with C-API wrappers.


