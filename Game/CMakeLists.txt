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

if(VCPKG_BUILD_TYPE STREQUAL "release" OR VCPKG_TARGET_TRIPLET MATCHES "-release$")
    add_custom_command(TARGET GoonSquad POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
            "$<TARGET_FILE_DIR:GoonSquad>"
    )
endif()

if(CMAKE_CXX_FLAGS MATCHES "fsanitize=address" OR CMAKE_CXX_FLAGS_INIT MATCHES "fsanitize=address")
    file(GLOB _asan_dll "$ENV{CLANG_P2996_ROOT}/lib/clang/*/lib/x86_64-pc-windows-msvc/clang_rt.asan_dynamic.dll")
    if(_asan_dll)
        list(GET _asan_dll 0 _asan_dll)
        add_custom_command(TARGET GoonSquad POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_asan_dll}" "$<TARGET_FILE_DIR:GoonSquad>"
        )
    else()
        message(WARNING "ASAN build requested but clang_rt.asan_dynamic.dll not found under $ENV{CLANG_P2996_ROOT}/lib/clang/*/lib/x86_64-pc-windows-msvc/. Run bootstrap.py to build compiler-rt.")
    endif()
endif()

