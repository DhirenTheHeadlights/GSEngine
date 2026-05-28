cmake_minimum_required(VERSION 4.2)

project(GSEditor)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Editor")
set(EDITOR_SRC "${EDITOR_DIR}/Source")
set(MAIN_CPP "${EDITOR_SRC}/Main.cpp")

file(GLOB_RECURSE EDITOR_MODULES CONFIGURE_DEPENDS "${EDITOR_DIR}/*.cppm")

add_library(EditorLib)

target_sources(EditorLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${EDITOR_DIR}"
        FILES ${EDITOR_MODULES}
)

target_include_directories(EditorLib PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Editor"
)

target_link_libraries(EditorLib PRIVATE Engine)

add_executable(Editor "${MAIN_CPP}")
target_link_libraries(Editor PRIVATE EditorLib Engine)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Editor PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Editor)
