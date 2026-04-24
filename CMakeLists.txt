cmake_minimum_required(VERSION 4.3)

if(NOT DEFINED ENV{CLANG_P2996_ROOT} OR "$ENV{CLANG_P2996_ROOT}" STREQUAL "")
    message(FATAL_ERROR "CLANG_P2996_ROOT is not set - run scripts/install_clang_p2996.py")
endif()

if(NOT DEFINED ENV{VCToolsInstallDir} OR NOT DEFINED ENV{WindowsSdkDir} OR NOT DEFINED ENV{WindowsSDKVersion} OR NOT DEFINED ENV{UniversalCRTSdkDir} OR NOT DEFINED ENV{UCRTVersion})
    message(FATAL_ERROR "MSVC env vars not set - launch from a Developer Command Prompt or Developer PowerShell")
endif()

file(TO_CMAKE_PATH "$ENV{CLANG_P2996_ROOT}" _gse_clang_root)

set(_gse_libcxx_inc "${_gse_clang_root}/include/c++/v1")
set(_gse_libcxx_lib "${_gse_clang_root}/lib/c++.lib")
if(NOT EXISTS "${_gse_libcxx_inc}/vector")
    message(FATAL_ERROR "libc++ headers not found at ${_gse_libcxx_inc} - run scripts/build_libcxx_p2996.py")
endif()

set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
    "${_gse_clang_root}/bin/clang-scan-deps.exe"
    CACHE FILEPATH "")

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdinc++ -Xclang -cxx-isystem -Xclang \"${_gse_libcxx_inc}\"")
string(APPEND CMAKE_CXX_FLAGS_INIT " -D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64")
string(APPEND CMAKE_CXX_FLAGS_INIT " -Xclang --dependent-lib=ucrt")
string(APPEND CMAKE_CXX_FLAGS_INIT " -D_STATIC_INLINE_UCRT_FUNCTIONS=0")
string(APPEND CMAKE_CXX_FLAGS_INIT " -freflection-latest")
string(APPEND CMAKE_C_FLAGS_INIT   " -Xclang --dependent-lib=ucrt")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT    " \"${_gse_libcxx_lib}\"")
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " \"${_gse_libcxx_lib}\"")

project(GSEngine)

set(_gse_libcxx_modules_dir "${_gse_clang_root}/share/libc++/v1")
if(NOT EXISTS "${_gse_libcxx_modules_dir}/std.cppm")
    message(FATAL_ERROR "libc++ std.cppm not found at ${_gse_libcxx_modules_dir} - rebuild libc++ with LIBCXX_INSTALL_MODULES=ON")
endif()

add_library(__cmake_libcxx_std STATIC)
set_property(TARGET __cmake_libcxx_std PROPERTY EXCLUDE_FROM_ALL 1)
set_property(TARGET __cmake_libcxx_std PROPERTY CXX_SCAN_FOR_MODULES 1)
set_property(TARGET __cmake_libcxx_std PROPERTY CXX_MODULE_STD 0)
target_compile_features(__cmake_libcxx_std PUBLIC cxx_std_26)
target_include_directories(__cmake_libcxx_std SYSTEM PRIVATE "${_gse_libcxx_inc}")
target_sources(__cmake_libcxx_std
    PUBLIC
        FILE_SET std TYPE CXX_MODULES
        BASE_DIRS "${_gse_libcxx_modules_dir}"
        FILES
            "${_gse_libcxx_modules_dir}/std.cppm"
            "${_gse_libcxx_modules_dir}/std.compat.cppm"
)
if(NOT TARGET __CMAKE::CXX23)
    add_library(__CMAKE::CXX23 ALIAS __cmake_libcxx_std)
endif()
if(NOT TARGET __CMAKE::CXX26)
    add_library(__CMAKE::CXX26 ALIAS __cmake_libcxx_std)
endif()

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)
add_subdirectory(Server)
