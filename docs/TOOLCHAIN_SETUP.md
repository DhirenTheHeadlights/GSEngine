# Toolchain Setup — clang-p2996 + libc++ + MSVC ABI on Windows

This repo targets Windows only but does **not** use the MSVC compiler or MSVC STL. We use clang-p2996 (Bloomberg's C++26 reflection fork of clang) with libc++ built against the MSVC ABI (`vcruntime`). Every vcpkg dep is rebuilt against this toolchain via a custom triplet.

If you need to set this up on a new machine, the short version is:

```
python bootstrap.py
```

This installs clang-p2996, builds libc++ against vcruntime, and leaves you ready to configure. What follows is *why* the bootstrap does what it does, and the landmines you'd hit if you tried to set it up by hand.

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

## 3. The MSVC C-headers-only shim

Problem: vcpkg port builds (and our own compile) inherit `INCLUDE` from vcvars, which puts MSVC's STL on the include path. That means `#include <vector>` could resolve to MSVC's `<vector>` instead of libc++'s — which conflicts with `import std;` and produces ambiguous redefinitions (especially via TBB headers).

Solution (in the root `CMakeLists.txt`, **before** `project()`):

1. Make a shim directory `${CMAKE_BINARY_DIR}/msvc-c-shim/include/`.
2. Copy only `*.h` files from `${VCToolsInstallDir}/include/` into the shim. This keeps C headers like `<stddef.h>`, `<stdint.h>` but drops the extensionless C++ STL files.
3. Override `INCLUDE` env:
   ```cmake
   set(ENV{INCLUDE} "${_gse_msvc_c_shim};${_gse_ucrt_inc}/ucrt;${_gse_winsdk_inc}/um;${_gse_winsdk_inc}/shared;${_gse_winsdk_inc}/winrt;${_gse_winsdk_inc}/cppwinrt")
   set(ENV{EXTERNAL_INCLUDE} "")
   ```
4. vcpkg has to inherit this — the custom triplet declares `VCPKG_ENV_PASSTHROUGH PATH INCLUDE LIB LIBPATH CLANG_P2996_ROOT`.

Without the shim, TBB's MSVC-STL-tainted headers bleed in and you get redefinition errors.

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

For the **vcpkg chainload toolchain** (clang-cl builds dep ports), the equivalent is:

```cmake
/clang:-nostdinc++ /clang:-cxx-isystem /clang:"<libcxx>"
```

## 5. Required defines and flags

```cmake
string(APPEND CMAKE_CXX_FLAGS_INIT " -D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64")
string(APPEND CMAKE_CXX_FLAGS_INIT " -Xclang --dependent-lib=ucrt")
string(APPEND CMAKE_C_FLAGS_INIT   " -Xclang --dependent-lib=ucrt")
```

- `__GCC_DESTRUCTIVE_SIZE` / `__GCC_CONSTRUCTIVE_SIZE` — libc++ needs these, but clang on MSVC target doesn't define them automatically. Without them, anything referencing `std::hardware_destructive_interference_size` (TBB headers, our own concurrency code) fails to compile.
- `--dependent-lib=ucrt` — ensures ucrt is linked into every TU. Required so libc++'s C runtime calls resolve.

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

## 7. vcpkg custom triplet + chainload

`vcpkg-overlays/triplets/x64-windows-clang-libcxx.cmake` pins dynamic CRT + chainloads our toolchain file:

```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME "")
set(VCPKG_ENV_PASSTHROUGH PATH INCLUDE LIB LIBPATH CLANG_P2996_ROOT)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/clang-libcxx.cmake")
```

`VCPKG_ENV_PASSTHROUGH INCLUDE` is the critical bit — without it, vcpkg resets INCLUDE and our MSVC-C shim doesn't apply inside port builds.

`vcpkg-overlays/toolchains/clang-libcxx.cmake` is the matching toolchain file — uses `clang-cl` with the `/clang:`-prefixed flags and links `c++.lib`.

## 8. vcpkg manifest quirks

`vcpkg.json` is standard manifest mode. Two non-obvious entries:

- `"tbb"` with `"default-features": false` — hwloc's `lstopo` fails to build under clang-cl because it uses `-mwindows` (gcc-ism), which clang-cl doesn't accept. Disabling default features skips hwloc.

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

After bootstrap, configure with the `x64-clang-p2996-libcxx-Debug` or `x64-clang-p2996-libcxx-Release` CMake preset. vcpkg rebuilds deps against the custom triplet the first time.

---

## If something breaks, in order

1. Check you're in Developer PowerShell (`echo $env:VCToolsInstallDir` — should be set).
2. Check `CLANG_P2996_ROOT` is set and points at `dist/clang-p2996/`.
3. Check `dist/clang-p2996/share/libc++/v1/std.cppm` exists. If not, libc++ module install failed — re-run `build_libcxx_p2996.py`.
4. Check `dist/clang-p2996/include/c++/v1/__new/new_handler.h` has the `get_new_handler` declaration inside `_LIBCPP_ABI_VCRUNTIME`.
5. Wipe `out/build/` and reconfigure. The INCLUDE override and C-shim are generated at configure time — stale build dirs can have the wrong paths baked in.
