cmake_minimum_required(VERSION 4.2)

project(SandboxProject)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SANDBOX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Sandbox")
set(SANDBOX_SRC "${SANDBOX_DIR}/Source")
set(MAIN_CPP "${SANDBOX_SRC}/Main.cpp")

file(GLOB_RECURSE SANDBOX_MODULES CONFIGURE_DEPENDS "${SANDBOX_DIR}/*.cppm")

file(GLOB_RECURSE SANDBOX_IMPLS CONFIGURE_DEPENDS "${SANDBOX_DIR}/*.cpp")
list(REMOVE_ITEM SANDBOX_IMPLS "${MAIN_CPP}")

# A .cpp that declares an implementation partition (module sandbox:y;) provides a
# module and must live in a CXX_MODULES file set; a plain implementation unit
# (module sandbox;) or global-module .cpp stays an ordinary source.
set(SANDBOX_MODULE_IMPLS "")
set(SANDBOX_PLAIN_IMPLS "")
foreach(_impl IN LISTS SANDBOX_IMPLS)
    file(STRINGS "${_impl}" _part_decl REGEX "^module sandbox:[A-Za-z_0-9]+ *;" LIMIT_COUNT 1)
    if(_part_decl)
        list(APPEND SANDBOX_MODULE_IMPLS "${_impl}")
    else()
        list(APPEND SANDBOX_PLAIN_IMPLS "${_impl}")
    endif()
endforeach()

add_library(SandboxLib)

target_sources(SandboxLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${SANDBOX_DIR}"
        FILES ${SANDBOX_MODULES}
)

if(SANDBOX_MODULE_IMPLS)
    target_sources(SandboxLib
        PRIVATE
            FILE_SET cxx_module_impls TYPE CXX_MODULES
            BASE_DIRS "${SANDBOX_DIR}"
            FILES ${SANDBOX_MODULE_IMPLS}
    )
endif()

if(SANDBOX_PLAIN_IMPLS)
    target_sources(SandboxLib PRIVATE ${SANDBOX_PLAIN_IMPLS})
endif()

target_include_directories(SandboxLib PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Sandbox"
)

target_link_libraries(SandboxLib PRIVATE Engine EngineServer)

add_executable(Sandbox "${MAIN_CPP}")
target_link_libraries(Sandbox PRIVATE SandboxLib Engine EngineServer)

# GCC-trunk's C++ modules codegen on MinGW (PE/COFF) emits vague-linkage function-local statics
# as STRONG symbols in every importing TU instead of COMDAT-folding them, so the final link sees
# duplicate definitions. The offenders are overwhelmingly in external code we can't touch:
# libstdc++ (`std::_Sp_make_shared_tag::_S_ti()::__tag`) and vulkan-hpp (`vk::errorCategory()::
# instance`, pulled in by its throwing API which the engine relies on). --allow-multiple-definition
# makes ld keep the first (identical) copy, i.e. it performs the COMDAT fold GCC failed to emit.
# Safe here; remove only once the GCC modules vague-linkage bug is fixed.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Sandbox PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Sandbox PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Sandbox)

