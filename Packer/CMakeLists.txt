cmake_minimum_required(VERSION 4.2)

project(GSEnginePacker)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PACKER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE PACKER_MODULES CONFIGURE_DEPENDS "${PACKER_SRC}/*.cppm")

add_executable(Packer "${PACKER_SRC}/Main.cpp")

target_sources(Packer
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${PACKER_SRC}"
        FILES ${PACKER_MODULES}
)

target_link_libraries(Packer PRIVATE Engine)

# Console subsystem on purpose: the transcript on stdout is what the editor and CI read.

# Same vague-linkage fold the Editor and Sandbox executables need; see Editor/CMakeLists.txt.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Packer PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Packer PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Packer)
