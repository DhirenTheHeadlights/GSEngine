cmake_minimum_required(VERSION 3.26)

project(GoonSquadProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Game")
set(GAME_SRC "${GAME_DIR}/Source")
set(MAIN_CPPM "${GAME_SRC}/Main.cppm")

file(GLOB_RECURSE GAME_MODULES CONFIGURE_DEPENDS "${GAME_DIR}/*.cppm")

list(REMOVE_ITEM GAME_MODULES "${MAIN_CPPM}")

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

add_executable(GoonSquad "${MAIN_CPPM}")
target_link_libraries(GoonSquad PRIVATE GoonSquadLib Engine)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2 /MP)
endif()
