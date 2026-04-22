# Toolchain Setup — clang-p2996 + libc++ + MSVC ABI on Windows

This repo targets Windows only. The **engine** uses clang-p2996 (Bloomberg's C++26 reflection fork of clang) with libc++ built against the MSVC ABI (`vcruntime`). **vcpkg deps** are built with stock MSVC (`x64-windows` triplet) — vcruntime ABI lets MSVC-compiled libs link cleanly into the clang+libc++ engine. This mixed-triplet approach sidesteps the libc++-vcruntime-ABI header gaps that otherwise bite when compiling third-party C++ under clang on Windows.

If you need to set this up on a new machine, the short version is:

```
python bootstrap.py
```

This installs clang-p2996, builds libc++ against vcruntime, builds compiler-rt (ASAN runtime), and leaves you ready to configure. What follows is *why* the bootstrap does what it does, and the landmines you'd hit if you tried to set it up by hand.

---

## Hard prerequisites

- **Developer PowerShell for VS 2026** (or Developer Command Prompt). Not a plain shell. You need `mt.exe` on PATH and `VCToolsInstallDir` / `WindowsSdkDir` / `WindowsSDKVersion` / `UniversalCRTSdkDir` / `UCRTVersion` in env. Configure will `FATAL_ERROR` if any are missing.
- **Vulkan SDK ≥ 1.3.296** installed (for `slang` / `slang.h`).
- **CMake ≥ 4.0**. Older versions can't drive `import std` with our experimental-flag dance.
- **CLANG_P2996_ROOT** env var pointing at `dist/clang-p2996/`. `install_clang_p2996.py` sets this; if you manually moved things, you must set it yourself — configure checks and hard-errors.

---

## 1. Why we build libc++ ourselves

clang-p2996 ships with libc++ headers but no libc++ library built against the MSVC ABI. We need:

- `LIBCXX_CXX_ABI=vcruntime` — use MSVC's EH runtime instead of libcxxabi. This is what lets libc++ interop with anything that throws through vcruntime (i.e. everything on Windows).
- `LIBCXX_INSTALL_MODULES=ON` — without this, `std.cppm` / `std.compat.cppm` don't get installed. Without those, `import std;` is dead.
- Compile libc++ with **clang-cl, NOT clang.exe**. When CMake detects clang-cl it sets `MSVC=TRUE`, which flips libcxx's `if(MSVC)` branch that wires up the correct ABI lib links. Use `clang.exe` and you'll get unresolved `__ExceptionPtr*` symbols at link time.

This is all baked into `scripts/build_libcxx_p2996.py`. It installs alongside the clang-p2996 tree so paths stay stable.

## 1a. Why we build compiler-rt ourselves

The clang-p2996 prebuilt doesn't ship a Windows compiler-rt runtime — no `clang_rt.asan_*.lib` or `.dll`. Without it, `-fsanitize=address` link fails with missing `clang_rt.asan_dynamic.lib`. We build ASAN runtime from the same `External/clang-p2996/compiler-rt/` source tree via the LLVM `runtimes/` subdir:

- `LLVM_ENABLE_RUNTIMES=compiler-rt` — use the runtimes build driver (not a full LLVM rebuild).
- `COMPILER_RT_SANITIZERS_TO_BUILD=asan` — only ASAN; skip MSAN/TSAN/fuzzer/XRay/profile/memprof/orc/crt/gwp-asan.
- `COMPILER_RT_BUILD_BUILTINS=OFF` — the prebuilt clang already has its builtins; rebuilding them would require extra MSVC-target dance.
- `COMPILER_RT_DEFAULT_TARGET_ONLY=ON` — only the host (x86_64-pc-windows-msvc), no cross-compilation variants.
- Built with clang-cl against the MSVC runtime (`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`), same shape as the libc++ build.

Install layout matches what clang expects:
- `$CLANG_P2996_ROOT/lib/clang/<ver>/lib/x86_64-pc-windows-msvc/clang_rt.asan_dynamic.lib`
- `$CLANG_P2996_ROOT/lib/clang/<ver>/lib/x86_64-pc-windows-msvc/clang_rt.asan_dynamic_runtime_thunk.lib`
- `$CLANG_P2996_ROOT/lib/clang/<ver>/lib/x86_64-pc-windows-msvc/clang_rt.asan_dynamic-x86_64.dll`

Baked into `scripts/build_compiler_rt_p2996.py`; chained after the libc++ build in `bootstrap.py`. The Game/CMakeLists.txt post-build step copies the DLL next to `GoonSquad.exe` when building with the `-asan` preset.

Historical note: earlier we tried to drop MSVC 14.50's ASAN libs from `VC\Tools\MSVC\...\lib\x64\` into the clang runtime path as a quick fix. That works at the ABI level (same compiler-rt sources) but pins the toolchain to a specific VS version. Building compiler-rt from the clang-p2996 source keeps the runtime locked to the clang version, which is the only guarantee worth having.

## 2. The libc++ header patch (`__new/new_handler.h`)

Libc++'s `_LIBCPP_ABI_VCRUNTIME` branch in `__new/new_handler.h` is broken — it forgets to declare `std::get_new_handler`, so anything pulling `<new>` through `import std` fails to resolve it.

We patch it in two places so rebuilds don't undo the fix:

- `dist/clang-p2996/include/c++/v1/__new/new_handler.h` (the shipped copy)
- `External/clang-p2996/libcxx/include/__new/new_handler.h` (the source)

The patch (inside the `_LIBCPP_ABI_VCRUNTIME` block):

```cpp
#if defined(_LIBCPP_ABI_VCRUNTIME)
#  include <new.h>
#  if defined(_MSC_VER) && !defined(_VCRTBLD)
#    if defined(_DEBUG)
#      pragma comment(lib, "msvcprtd")
#    else
#      pragma comment(lib, "msvcprt")
#    endif
#  endif
_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
using ::new_handler;
using ::set_new_handler;
extern "C++" __declspec(dllimport) new_handler __cdecl get_new_handler() _NOEXCEPT;
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD
```

If you rebuild libc++ from scratch without applying the source patch first, the shipped copy ends up regenerated and broken.

## 3. Why there's no MSVC-STL shim (and why we don't need one)

Naive expectation: vcvars puts `${VCToolsInstallDir}/include` on `INCLUDE`, which contains MSVC's `<vector>`, `<functional>`, etc. A `#include <vector>` in engine code could resolve to MSVC's instead of libc++'s, colliding with `import std;`.

In practice this doesn't happen, because of how clang's include search is ordered when §4's flags are applied:

- `-nostdinc++` suppresses clang's default C++ include paths.
- `-Xclang -cxx-isystem -Xclang <libcxx>` prepends libc++ to the C++-only search chain, which is consulted *before* the regular system-include chain.
- `INCLUDE` (vcvars) is injected into the regular system-include chain — behind the C++-only chain.

Net: libc++ wins every `#include <vector>` lookup in engine code, and MSVC's STL headers in VCTools/include are invisible to C++ TUs compiled with clang. C headers (`<stddef.h>`, `<intrin.h>`, etc.) resolve to VCTools/include because libc++ has no `.h` counterparts to shadow them — exactly what we want.

vcpkg deps compile with MSVC cl.exe and use MSVC STL unchanged; they never see libc++. vcruntime ABI makes the resulting `.lib`/`.dll` artifacts link-compatible with the clang+libc++ engine.

## 4. The libc++ include flag quirks

These all look like they should work on Windows MSVC target but silently don't:

- `-stdlib=libc++` — silently ignored. No error.
- `-stdlib++-isystem <path>` — silently ignored by `clang-scan-deps` (so modules break even if the main compile is fine).
- `--vctoolsdir=` / `--winsdkdir=` — clang-cl only. The `clang.exe` driver rejects them.

**What actually works** (in `CMakeLists.txt`):

```cmake
string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdinc++ -Xclang -cxx-isystem -Xclang \"${_gse_libcxx_inc}\"")
```

`-nostdinc++` prevents clang from auto-adding its own libc++ include, then `-Xclang -cxx-isystem -Xclang <path>` injects ours first in the C++-include search. `-cxx-isystem` (not `-isystem`) is important so the C include path stays unchanged.

vcpkg deps don't need these — they build with MSVC cl.exe + MSVC STL and are self-contained.

## 5. Required defines and flags

```cmake
string(APPEND CMAKE_CXX_FLAGS_INIT " -D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64")
string(APPEND CMAKE_CXX_FLAGS_INIT " -Xclang --dependent-lib=ucrt")
string(APPEND CMAKE_C_FLAGS_INIT   " -Xclang --dependent-lib=ucrt")
string(APPEND CMAKE_CXX_FLAGS_INIT " -freflection-latest")
```

- `__GCC_DESTRUCTIVE_SIZE` / `__GCC_CONSTRUCTIVE_SIZE` — libc++ needs these, but clang on MSVC target doesn't define them automatically. Without them, anything referencing `std::hardware_destructive_interference_size` in our own concurrency code fails to compile.
- `--dependent-lib=ucrt` — ensures ucrt is linked into every TU. Required so libc++'s C runtime calls resolve.
- `-freflection-latest` — enables the full P2996 reflection feature set (superset of `-freflection`): `^^` operator, `std::meta::info`, `template for`, `[[= value]]` annotations, `define_aggregate`, splicers. Without it the parser rejects `^^T` as a syntax error. Not applied to the vcpkg chainload toolchain — third-party deps don't use reflection, so they compile without it.

**Note on `std::meta` and `import std;`:** Bloomberg's `std.cppm` `#include`s `<meta>` in the global module fragment but ships no `std/meta.inc`, so `import std;` does **not** re-export `std::meta::*` names. In any TU that uses reflection, add `#include <meta>` alongside `import std;`. The names aren't exported from the module, so there's no collision.

## 6. `import std` integration

CMake gates `import std` behind an experimental flag:

```cmake
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "" FORCE)
```

The UUID changes per CMake version. If you bump CMake, grep for `EXPERIMENTAL_CXX_IMPORT_STD` in the CMake source to find the new one.

We then ship our own `__CMAKE::CXX26` alias target pointing at our libc++ `std.cppm` / `std.compat.cppm`:

```cmake
add_library(__cmake_libcxx_std STATIC)
set_property(TARGET __cmake_libcxx_std PROPERTY CXX_SCAN_FOR_MODULES 1)
set_property(TARGET __cmake_libcxx_std PROPERTY CXX_MODULE_STD 0)
target_compile_features(__cmake_libcxx_std PUBLIC cxx_std_26)
target_sources(__cmake_libcxx_std PUBLIC
    FILE_SET std TYPE CXX_MODULES
    BASE_DIRS "${_gse_libcxx_modules_dir}"
    FILES
        "${_gse_libcxx_modules_dir}/std.cppm"
        "${_gse_libcxx_modules_dir}/std.compat.cppm"
)
add_library(__CMAKE::CXX26 ALIAS __cmake_libcxx_std)
```

`CXX_MODULE_STD 0` on this target is what stops CMake from trying to synthesize a different `import std` module for it — we're providing the implementation.

## 7. vcpkg triplets

Two triplets are in play depending on preset:

- **`x64-windows`** (stock, default) — used by `x64-clang-p2996-libcxx-Debug` and `x64-clang-p2996-libcxx-Release`. Deps build with MSVC cl.exe and MSVC STL, shipping both Debug (`/MDd`, `msvcrtd`) and Release (`/MD`, `msvcrt`) variants. The Debug variant matches the engine's debug CRT for normal development.
- **`x64-windows-release`** (overlay, custom) — used by `x64-clang-p2996-libcxx-Debug-asan` only. Defined in `vcpkg-overlays/triplets/x64-windows-release.cmake`:
  ```cmake
  set(VCPKG_TARGET_ARCHITECTURE x64)
  set(VCPKG_CRT_LINKAGE dynamic)
  set(VCPKG_LIBRARY_LINKAGE dynamic)
  set(VCPKG_BUILD_TYPE release)
  ```
  Forces deps to build **release-only**, so the whole process links `msvcrt.dll` (not `msvcrtd.dll`). Required because MSVC's `/fsanitize=address` — and clang's, for the same reason — is incompatible with the Debug CRT: `ucrtbased.dll`'s `recalloc_dbg` / `_initterm_e` allocate through a separate debug-heap-tracking path that ASAN's `realloc` interceptor can't see, triggering bad-free reports during DLL init.

When switching between Debug and Debug+ASAN, vcpkg will rebuild deps once under the other triplet and then binary-cache them.

Historical note: an earlier iteration attempted a custom `x64-windows-clang-libcxx` triplet that chainloaded clang-cl + libc++ for dep builds (full STL swap). That exposed latent libc++-vcruntime-ABI bugs (e.g., `std::get_new_handler` missing from UCRT's `<new.h>` while declared-only in MSVC STL's `<new>`), requiring libc++ header patches and forced `msvcprt.lib` linkage. That path is still abandoned — the current `x64-windows-release` overlay is a narrower change (only flips CRT flavor, keeps MSVC STL for deps). The libc++ `__new/new_handler.h` patch in `build_libcxx_p2996.py` is kept in place so the engine can safely call `std::get_new_handler()` if needed.

### Latent mixed-CRT heap corruption

A subtle side effect of the default setup is worth documenting: libc++ is built with `MultiThreadedDLL` (release CRT per `build_libcxx_p2996.py`), vcpkg deps under `x64-windows` link `msvcrtd` in Debug, and the engine itself linked `msvcrtd`. In the same process three heap wrappers coexist — release ucrt under libc++, debug ucrt under deps/engine — with different block-header layouts. Allocations crossing libc++↔dep boundaries could produce a free/realloc with a header the runtime doesn't recognize, yielding random heap corruption that only surfaced when a `std::vector` grew and tried to free the old buffer. The `x64-windows-release` ASAN preset eliminates this (everything on release CRT). The default Debug preset still mixes CRT flavors; if you see unexplained heap corruption, switching to the ASAN preset may resolve it without finding a code bug.

## 8. vcpkg manifest quirks

`vcpkg.json` is standard manifest mode. No unusual flags — all entries use default features.

## 9. The `hypotf` / `ldexpf` shim (`Engine/Modules/MathShims.cpp`)

Libc++'s `<__math/...>` headers call bare `::hypotf(x, y)` and `::ldexpf(x, n)`. But ucrt's `<math.h>` declares these as inline wrappers around `_hypotf` / a cast through `ldexp` — meaning the real exported symbols are `_hypotf` and `ldexp`, while `hypotf` / `ldexpf` only exist when `<math.h>` is included and the inline is emitted.

When `import std;` compiles libc++ into a module BMI, those bare calls end up as unresolved external symbol references at link time.

Fix: a tiny TU that provides the symbols directly (no `<math.h>` include, to avoid redefinition with the inline):

```cpp
extern "C" {
    float _hypotf(float, float);
    double ldexp(double, int);

    float hypotf(float x, float y) {
        return _hypotf(x, y);
    }
    float ldexpf(float x, int n) {
        return static_cast<float>(ldexp(static_cast<double>(x), n));
    }
}
```

Things that do **not** work as alternatives:
- `-fno-builtin-hypotf -fno-builtin-ldexpf` — that flag only stops the optimizer from auto-recognizing math patterns; it doesn't suppress direct source-level calls.
- Including `<math.h>` in the shim — triggers redefinition against ucrt's inline.

The shim is added via `target_sources(Engine PRIVATE Modules/MathShims.cpp)`.

## 10. Vulkan-Hpp dynamic dispatch storage (`Engine/Modules/VulkanDispatch.cpp`)

`VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE` must live in exactly one TU. Ours is in `Engine/Modules/VulkanDispatch.cpp`, attached to the `VulkanModule` target. Don't put it in a module interface — it needs to be a plain object file so the linker finds it once.

---

## 11. clang-p2996 codegen crash — `std::ranges::sort` with member-pointer projection

Not strictly toolchain, but worth recording because it ate a full debug session: at `-O3` with reflection flags on, compiling a function in a module purview that contains:

```cpp
std::ranges::sort(v, {}, &some_struct::member);
```

…crashes clang-p2996 with a 0xC0000005 access violation during `LLVM IR generation of declaration`. The crash moves to whichever function contains the call if you refactor, but the call itself is the trigger. Workaround:

```cpp
std::ranges::sort(v, [](const auto& a, const auto& b) {
    return a.member < b.member;
});
```

Projection-via-lambda (`, {}, [](auto const& x) { return x.member; }`) may have the same problem — we didn't test it exhaustively. Plain comparator is safest.

---

## Full bootstrap sequence (what `bootstrap.py` does)

1. `git submodule update --init` — pulls the `External/clang-p2996` source.
2. `python scripts/install_clang_p2996.py` — builds clang-p2996 and installs to `dist/clang-p2996/`, sets `CLANG_P2996_ROOT`.
3. Patches `External/clang-p2996/libcxx/include/__new/new_handler.h` (see §2).
4. `python scripts/build_libcxx_p2996.py` — builds libc++ with `LIBCXX_CXX_ABI=vcruntime`, `LIBCXX_INSTALL_MODULES=ON`, using clang-cl. Installs into `dist/clang-p2996/`.
5. Patches the installed `dist/clang-p2996/include/c++/v1/__new/new_handler.h` (same patch, in case it got overwritten by install).

After bootstrap, configure with the `x64-clang-p2996-libcxx-Debug` or `x64-clang-p2996-libcxx-Release` CMake preset. vcpkg builds deps under the stock `x64-windows` triplet the first time (or restores from binary cache).

---

## If something breaks, in order

1. Check you're in Developer PowerShell (`echo $env:VCToolsInstallDir` — should be set).
2. Check `CLANG_P2996_ROOT` is set and points at `dist/clang-p2996/`.
3. Check `dist/clang-p2996/share/libc++/v1/std.cppm` exists. If not, libc++ module install failed — re-run `build_libcxx_p2996.py`.
4. Check `dist/clang-p2996/include/c++/v1/__new/new_handler.h` has the `get_new_handler` declaration inside `_LIBCPP_ABI_VCRUNTIME`.
5. Wipe `out/build/` and reconfigure. Stale build dirs can have the wrong paths baked into module BMIs or the compile_commands database.
