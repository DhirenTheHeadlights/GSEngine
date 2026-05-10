cmake_minimum_required(VERSION 4.2)

project(GoonSquadProject)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Game")
set(GAME_SRC "${GAME_DIR}/Source")
set(MAIN_CPP "${GAME_SRC}/Main.cpp")

file(GLOB_RECURSE GAME_MODULES CONFIGURE_DEPENDS "${GAME_DIR}/*.cppm")

add_library(GoonSquadLib)

target_sources(GoonSquadLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${GAME_DIR}"
        FILES ${GAME_MODULES}
)

target_include_directories(GoonSquadLib PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Game"
)

target_link_libraries(GoonSquadLib PRIVATE Engine)

add_executable(GoonSquad "${MAIN_CPP}")
target_link_libraries(GoonSquad PRIVATE GoonSquadLib Engine)
add_dependencies(GoonSquad ShaderCodegenRun)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(GoonSquad PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(GoonSquad)

