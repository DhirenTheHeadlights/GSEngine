cmake_minimum_required(VERSION 3.26)

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

if(AFTERMATH_DLL)
    add_custom_command(TARGET GoonSquad POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AFTERMATH_DLL}" "$<TARGET_FILE_DIR:GoonSquad>"
    )
endif()

