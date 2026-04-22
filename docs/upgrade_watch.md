# Upgrade Watch — Workarounds waiting on upstream

Each item is a local hack, patch, or gymnastic dance we're doing because upstream hasn't landed the fix yet. When the trigger condition is met, the workaround can be deleted.

Format per item: **summary** → why it's there → unblock condition → files to touch → verify check.

---

## 1. `VulkanDispatch.cpp` — manual dispatcher storage TU

- **What:** `Engine/Modules/VulkanDispatch.cpp` contains nothing but `VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE`. Linked into `VulkanModule` so `defaultDispatchLoaderDynamic` has a definition.
- **Why:** Vulkan-Hpp ≤ 1.4.341 ships `vulkan.cppm` but the module doesn't invoke the storage macro itself, so one TU must expand it.
- **Unblock when:** LunarG Vulkan SDK ships Vulkan-Hpp ≥ 1.4.344. The module auto-provides storage.
- **Files to touch:** delete `Engine/Modules/VulkanDispatch.cpp`, drop the first argument from `add_library(VulkanModule STATIC "…/VulkanDispatch.cpp")` in `Engine/CMakeLists.txt`, update `docs/TOOLCHAIN_SETUP.md` §10.
- **Verify:** `grep -n "VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE" $Vulkan_INCLUDE_DIR/vulkan/vulkan.cppm` — if the macro is invoked (not just defined) in `vulkan.cppm`, storage is automatic.

## 2. `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` UUID dance

- **What:** Top-level `CMakeLists.txt` sets `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444"` to unlock `import std`.
- **Why:** CMake 3.30–4.2 gate `import std` behind a version-specific UUID.
- **Unblock when:** CMake ≥ 4.3 is the minimum we care about (both standalone and VS-bundled).
- **Files to touch:** delete the `set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD …)` line in `CMakeLists.txt`; bump `cmake_minimum_required` to 4.3.
- **Verify:** `cmake --version` ≥ 4.3 and VS-bundled CMake (`Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`) ≥ 4.3.

## 3. libc++ `__new/new_handler.h` patch (vcruntime ABI)

- **What:** `scripts/build_libcxx_p2996.py` patches libc++'s `__new/new_handler.h` to declare an inline `std::get_new_handler()` in the `_LIBCPP_ABI_VCRUNTIME` branch.
- **Why:** libc++ in vcruntime-ABI mode includes UCRT's `<new.h>`, which only declares `set_new_handler`. `get_new_handler` lives in MSVC STL's `<new>`, which libc++ doesn't pull in. Any code calling `std::get_new_handler()` fails to compile under this toolchain.
- **Unblock when:** upstream libc++ declares `get_new_handler` itself in the vcruntime-ABI path (or provides its own implementation).
- **Files to touch:** delete `NEW_HANDLER_OLD` / `NEW_HANDLER_NEW` / `patch_libcxx_get_new_handler` from `scripts/build_libcxx_p2996.py`; rebuild libc++ (or manually revert the patch in `$CLANG_P2996_ROOT/include/c++/v1/__new/new_handler.h`).
- **Verify:** upstream libc++ HEAD in `External/clang-p2996/libcxx/include/__new/new_handler.h` already declares `get_new_handler` inside `_LIBCPP_ABI_VCRUNTIME` — if so, the patch is a no-op and can be removed.

## 4. `hypotf` / `ldexpf` math shim

- **What:** `Engine/Modules/MathShims.cpp` provides `::hypotf` and `::ldexpf` wrappers delegating to `_hypotf` and `ldexp`.
- **Why:** libc++'s `<__math/...>` calls bare `::hypotf` / `::ldexpf`, but UCRT's `<math.h>` only declares them as inline wrappers around `_hypotf` / a cast through `ldexp`. Under `import std;` those inlines never emit, and the link fails.
- **Unblock when:** either libc++ stops bare-calling these (or uses the `__builtin_hypotf` / `__builtin_ldexpf` intrinsics), or clang auto-links an implementation on MSVC target.
- **Files to touch:** delete `Engine/Modules/MathShims.cpp` and the `target_sources(Engine PRIVATE "…/MathShims.cpp")` line in `Engine/CMakeLists.txt`. Remove `docs/TOOLCHAIN_SETUP.md` §9.
- **Verify:** try the deletion, do a clean build with `import std;`. If no unresolved `hypotf` / `ldexpf` symbols, the shim is dead.

## 5. `__GCC_DESTRUCTIVE_SIZE` / `__GCC_CONSTRUCTIVE_SIZE` defines

- **What:** Top-level `CMakeLists.txt` manually defines these to 64.
- **Why:** libc++ uses them for `std::hardware_destructive_interference_size` / `hardware_constructive_interference_size`, but clang on MSVC target doesn't define them automatically.
- **Unblock when:** clang-p2996 (or upstream clang) adds these to the default predefined macros on MSVC target.
- **Files to touch:** delete the `string(APPEND CMAKE_CXX_FLAGS_INIT " -D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64")` line.
- **Verify:** `clang --target=x86_64-pc-windows-msvc -dM -E - < /dev/null | grep DESTRUCTIVE_SIZE` — if clang predefines them, we don't need to.

## 6. `--dependent-lib=ucrt` flags

- **What:** Top-level `CMakeLists.txt` passes `-Xclang --dependent-lib=ucrt` for both C and C++.
- **Why:** clang on MSVC target doesn't auto-link UCRT, so libc++'s C runtime calls go unresolved.
- **Unblock when:** clang defaults to linking UCRT when `-fms-compatibility` or the MSVC target is in use.
- **Files to touch:** delete the two `--dependent-lib=ucrt` lines.
- **Verify:** try removing, link a simple `import std;` main. If no unresolved C runtime symbols, flag is dead.

## 7. clang-p2996 codegen crash — `std::ranges::sort` with member-pointer projection

- **What:** `docs/TOOLCHAIN_SETUP.md` §11 documents a 0xC0000005 crash in clang-p2996 when compiling `std::ranges::sort(v, {}, &some_struct::member)` inside a module at `-O3`. Workaround is a comparator lambda.
- **Why:** clang-p2996 codegen bug triggered by member-pointer projections in reflection-enabled module TUs.
- **Unblock when:** Bloomberg merges / upstream clang fixes the IR-generation crash. Track via clang-p2996 releases.
- **Files to touch:** search the codebase for the workaround pattern and consider reverting to the canonical `std::ranges::sort(v, {}, &T::member)` form once the bug is fixed. Remove §11 from `docs/TOOLCHAIN_SETUP.md`.
- **Verify:** minimal repro — `import std;` module TU with one `ranges::sort(v, {}, &S::x)` call, `-O3 -freflection-latest`. If it compiles, the bug is gone.

## 8. Custom clang-p2996 + prebuilt libc++ + compiler-rt bootstrap

- **What:** `bootstrap.py` + `scripts/install_clang_p2996.py` + `scripts/build_libcxx_p2996.py` + `scripts/build_compiler_rt_p2996.py`. Downloads a prebuilt clang-p2996, clones upstream source, patches libc++, builds libc++ locally against vcruntime ABI, then builds compiler-rt (ASAN runtime) into the same clang install dir.
- **Why:** P2996 reflection isn't in any released clang, clang-p2996 doesn't ship a Windows/vcruntime libc++ build, and the prebuilt clang-p2996 doesn't include compiler-rt runtimes either.
- **Unblock when:** P2996 reflection merges into mainline clang + LLVM ships Windows libc++ binaries with vcruntime ABI + clang-p2996 prebuilds include compiler-rt. Realistically multiple years out.
- **Files to touch:** retire `bootstrap.py`, all three scripts, `.github/workflows/build-clang-p2996.yml`, `CLANG_P2996_ROOT` env var plumbing. Switch `CMakeLists.txt` to stock clang.
- **Verify:** clang mainline release notes mention P2996 / `std::meta` as non-experimental.

## 9. `x64-windows-release` overlay triplet for ASAN

- **What:** `vcpkg-overlays/triplets/x64-windows-release.cmake` + `VCPKG_BUILD_TYPE=release`. Used only by the `x64-clang-p2996-libcxx-Debug-asan` preset to force vcpkg deps onto release CRT (`/MD`, `msvcrt.dll`) instead of debug CRT (`/MDd`, `msvcrtd.dll`).
- **Why:** ASAN on Windows is fundamentally incompatible with the debug CRT — `ucrtbased.dll`'s `recalloc_dbg` / `_initterm_e` heap-tracking paths trigger ASAN bad-free reports during DLL init before `main()` runs. Microsoft's own `/fsanitize=address` has the same restriction ("cannot combine with /MDd"). Unifying engine + deps on release CRT sidesteps this entirely.
- **Unblock when:** either (a) ASAN gains proper debug-CRT interoperability upstream (unlikely given MSVC's own docs still forbid the combination), or (b) clang adds a flag to suppress its interception of debug-CRT-internal allocators.
- **Files to touch:** delete `vcpkg-overlays/triplets/x64-windows-release.cmake`, drop the `VCPKG_TARGET_TRIPLET` / `VCPKG_OVERLAY_TRIPLETS` cache vars from the ASAN preset in `CMakePresets.json`, drop the `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` line from the same preset, drop the vcpkg-bin DLL-copy block at the bottom of `Game/CMakeLists.txt`, update `docs/TOOLCHAIN_SETUP.md` §7 to remove the overlay discussion.
- **Verify:** build the ASAN preset against stock `x64-windows` with `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL`. If `GoonSquad.exe` boots without a `recalloc_dbg`-path ASAN report, the workaround is unnecessary.

---

## How to use this doc

When something here becomes actionable:

1. Confirm the unblock condition (version check, upstream commit, etc.).
2. Apply the deletion in the listed files.
3. Delete the corresponding item from this doc.
4. If the item had a `docs/TOOLCHAIN_SETUP.md` section, remove or trim that too.

Every entry should eventually be deletable. If something is permanent and not waiting on anything, it belongs in `TOOLCHAIN_SETUP.md`, not here.
