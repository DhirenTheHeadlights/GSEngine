# vcpkg chainload toolchain for the prebuilt native-Windows gcc-trunk MinGW
# compiler. vcpkg's stock scripts/toolchains/mingw.cmake (a) finds the compiler
# via find_program on PATH and (b) sets CMAKE_<LANG>_COMPILER_TARGET. Neither
# works for this native gcc: the bin isn't on PATH during a build-triggered
# reconfigure, and native GCC rejects the --target= cross-compile flag. This toolchain
# fixes both, then defers to mingw.cmake for the remaining MinGW setup.
#
# Mirrors how the top-level CMakePresets locate the compiler:
# $env{USERPROFILE}/.gcc-trunk/current (a junction maintained by the installer).
# USERPROFILE must be in the triplet's VCPKG_ENV_PASSTHROUGH to be visible here.

set(_gcc_trunk_bin "$ENV{USERPROFILE}/.gcc-trunk/current/bin")

# The compiler driver spawns cc1/cc1plus, which load the toolchain's runtime DLLs
# (libisl, libmpc, ...) from bin. They are NOT alongside cc1, so bin must be on
# PATH for the compiler to run at all. Set it here (native separators) so it is
# present regardless of how cmake was launched.
file(TO_NATIVE_PATH "${_gcc_trunk_bin}" _gcc_trunk_bin_native)
set(ENV{PATH} "${_gcc_trunk_bin_native};$ENV{PATH}")

set(CMAKE_C_COMPILER   "${_gcc_trunk_bin}/x86_64-w64-mingw32-gcc.exe" CACHE FILEPATH "gcc-trunk C compiler"   FORCE)
set(CMAKE_CXX_COMPILER "${_gcc_trunk_bin}/x86_64-w64-mingw32-g++.exe" CACHE FILEPATH "gcc-trunk C++ compiler" FORCE)
set(CMAKE_RC_COMPILER  "${_gcc_trunk_bin}/windres.exe"               CACHE FILEPATH "gcc-trunk RC compiler"  FORCE)

# Path relative to this file (repo/vcpkg-overlays/triplets) rather than
# _VCPKG_ROOT_DIR, which isn't propagated into try_compile sub-builds.
include("${CMAKE_CURRENT_LIST_DIR}/../../Engine/External/vcpkg/scripts/toolchains/mingw.cmake")

# Native GCC has no --target= flag. mingw.cmake sets a cross
# target triple; clear it so CMake can identify the compiler as GNU.
unset(CMAKE_C_COMPILER_TARGET CACHE)
unset(CMAKE_CXX_COMPILER_TARGET CACHE)
set(CMAKE_C_COMPILER_TARGET "" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_TARGET "" CACHE STRING "" FORCE)
