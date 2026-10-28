# The wasm32 cross toolchain Tools/build_gcc_wasm.py builds, used straight from its build
# directory: nothing there runs `make install`, so the driver is xg++ with -B naming the
# directory cc1plus sits in, and libstdc++'s headers are named the way target_cxxflags in
# that script names them. Every path below is one that script derives too; the snapshot is
# its DEFAULT_SNAPSHOT and has to move with it.
#
# The compiler is never asked to identify itself or to link a test program. Its driver
# reaches the assembler and the linker through the build tree's exec-tool shims, which are
# shell scripts no Ninja command can run, so CMake's identification compile would fail
# before it learned anything. The identity is stated instead, and every rule that would
# have gone through a shim is spelled out in wasm32-wasi-gcc-rules.cmake.

set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(GSE_WASM_GCC_SNAPSHOT "20260809" CACHE STRING "GCC snapshot the wasm32 build directory was configured from")

get_filename_component(_gse_repo "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(GSE_WASM_WORKDIR "${_gse_repo}/.generated/.gcc-wasm-build")
set(GSE_WASM_GCC_DIR "${GSE_WASM_WORKDIR}/build/gcc")
set(GSE_WASM_TARGET_DIR "${GSE_WASM_WORKDIR}/build/wasm32-unknown-none")
set(GSE_WASM_GCC_SOURCE "${GSE_WASM_WORKDIR}/gcc-17-${GSE_WASM_GCC_SNAPSHOT}")
set(GSE_WASM_SYSROOT_LIB "${GSE_WASM_WORKDIR}/sysroot/lib/wasm32-wasi")
set(GSE_WASM_HOST_BIN "${_gse_repo}/.generated/msys64/mingw64/bin")

# The shared memory the launcher and the page instantiate, in 64 KiB wasm pages. The link's
# --max-memory is derived from the maximum, so the module and its hosts cannot disagree.
set(GSE_WASM_INITIAL_PAGES 256)
set(GSE_WASM_MAX_PAGES 8192)

set(CMAKE_C_COMPILER "${GSE_WASM_GCC_DIR}/xgcc.exe")
set(CMAKE_CXX_COMPILER "${GSE_WASM_GCC_DIR}/xg++.exe")
set(CMAKE_AR "${GSE_WASM_HOST_BIN}/llvm-ar.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${GSE_WASM_HOST_BIN}/llvm-ranlib.exe" CACHE FILEPATH "" FORCE)

foreach(_lang C CXX)
    set(CMAKE_${_lang}_COMPILER_ID_RUN TRUE)
    set(CMAKE_${_lang}_COMPILER_ID GNU)
    set(CMAKE_${_lang}_COMPILER_VERSION 17.0.0)
    set(CMAKE_${_lang}_COMPILER_FRONTEND_VARIANT GNU)
    set(CMAKE_${_lang}_EXTENSIONS_COMPUTED_DEFAULT ON)
endforeach()
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT 23)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT 17)
set(CMAKE_CXX_STANDARD_LIBRARY libstdc++)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".wasm")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX "${CMAKE_CURRENT_LIST_DIR}/wasm32-wasi-gcc-rules.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C "${CMAKE_CURRENT_LIST_DIR}/wasm32-wasi-gcc-rules.cmake")

set(_gse_wasm_cxx_include "${GSE_WASM_TARGET_DIR}/libstdc++-v3/include")
string(JOIN " " CMAKE_CXX_FLAGS_INIT
    "-B${GSE_WASM_GCC_DIR}/"
    "-fno-rtti"
    "-nostdinc++"
    "-I${_gse_wasm_cxx_include}/wasm32-unknown-none"
    "-I${_gse_wasm_cxx_include}"
    "-I${_gse_wasm_cxx_include}/backward"
    "-I${GSE_WASM_GCC_SOURCE}/libstdc++-v3/libsupc++"
    "-D_WASI_EMULATED_SIGNAL"
    "-D_Thread_local=__thread")
string(JOIN " " CMAKE_C_FLAGS_INIT
    "-B${GSE_WASM_GCC_DIR}/"
    "-D_WASI_EMULATED_SIGNAL")

# libstdc++'s `std' module, as build_std_module assembles it: the two source-tree .in files
# concatenated, not the build tree's std.cc, which libstdc++'s own recipe empties when the
# module fails to compile. CMake builds import std from whatever this manifest names.
# Copied only when different, so a reconfigure does not touch std.cc and rebuild every
# module behind it.
if(NOT CMAKE_IN_TRY_COMPILE)
    set(_gse_std_dir "${CMAKE_BINARY_DIR}/wasm-std")
    set(_gse_std_in "${GSE_WASM_GCC_SOURCE}/libstdc++-v3/src/c++23")
    file(READ "${_gse_std_in}/std.cc.in" _gse_std_main)
    file(READ "${_gse_std_in}/std-clib.cc.in" _gse_std_clib)
    file(WRITE "${_gse_std_dir}/std.cc.next" "${_gse_std_main}${_gse_std_clib}")
    file(COPY_FILE "${_gse_std_dir}/std.cc.next" "${_gse_std_dir}/std.cc" ONLY_IF_DIFFERENT)
    file(WRITE "${_gse_std_dir}/libstdc++.modules.json.next"
        "{\"version\": 1, \"revision\": 1, \"modules\": [{\"logical-name\": \"std\", \"source-path\": \"std.cc\", \"is-std-library\": true}]}\n")
    file(COPY_FILE "${_gse_std_dir}/libstdc++.modules.json.next" "${_gse_std_dir}/libstdc++.modules.json" ONLY_IF_DIFFERENT)
    set(CMAKE_CXX_STDLIB_MODULES_JSON "${_gse_std_dir}/libstdc++.modules.json")
endif()
