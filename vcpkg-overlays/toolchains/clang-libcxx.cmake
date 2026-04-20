if(NOT DEFINED ENV{CLANG_P2996_ROOT})
    message(FATAL_ERROR "CLANG_P2996_ROOT is not set")
endif()

file(TO_CMAKE_PATH "$ENV{CLANG_P2996_ROOT}" _clang_root)
set(_clang_cl   "${_clang_root}/bin/clang-cl.exe")
set(_libcxx_inc "${_clang_root}/include/c++/v1")
set(_libcxx_lib "${_clang_root}/lib/c++.lib")

set(CMAKE_C_COMPILER   "${_clang_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_clang_cl}" CACHE FILEPATH "" FORCE)

set(CMAKE_C_FLAGS_INIT   "-m64" CACHE STRING "")
set(CMAKE_CXX_FLAGS_INIT "-m64 /clang:-nostdinc++ /clang:-cxx-isystem /clang:\"${_libcxx_inc}\"" CACHE STRING "")

set(CMAKE_EXE_LINKER_FLAGS_INIT    "\"${_libcxx_lib}\"" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "\"${_libcxx_lib}\"" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "\"${_libcxx_lib}\"" CACHE STRING "")
