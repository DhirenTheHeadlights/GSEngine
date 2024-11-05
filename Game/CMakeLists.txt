cmake_minimum_required(VERSION 3.16)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the project
project(GoonSquadProject)

# Option for production build
option(PRODUCTION_BUILD "Make this a production build" OFF)

# Include directories
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Include")

# Gather game source and header files
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS "${INCLUDE_DIR}/*.h")

# GoonSquadLib: Add as a static library
add_library(GoonSquadLib STATIC ${GAME_SOURCES} ${GAME_HEADERS})

# Include directories for GoonSquadLib
target_include_directories(GoonSquadLib PUBLIC "${INCLUDE_DIR}")

# Link against the Engine for GoonSquadLib
target_link_libraries(GoonSquadLib PRIVATE Engine)

# GoonSquad: Add as an executable
add_executable(GoonSquad ${GAME_SOURCES} ${GAME_HEADERS})

# Include directories for GoonSquad
target_include_directories(GoonSquad PUBLIC "${INCLUDE_DIR}")

# Link against GoonSquadLib and Engine for GoonSquad
target_link_libraries(GoonSquad PRIVATE GoonSquadLib Engine)

# Define macros for resource paths based on build type
if(PRODUCTION_BUILD)
    target_compile_definitions(GoonSquadLib PUBLIC RESOURCES_PATH="./Game/Resources/" PRODUCTION_BUILD=1)
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="./Game/Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(GoonSquadLib PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Game/Resources/" PRODUCTION_BUILD=0)
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Game/Resources/" PRODUCTION_BUILD=0)
endif()

# MSVC-specific compile options
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

    # (Optional) Function to create source groups for Visual Studio
    function(create_source_groups BASE_DIR REMOVE_PREFIX BASE_GROUP)
        file(GLOB_RECURSE children RELATIVE ${BASE_DIR} ${BASE_DIR}/*)
        foreach(child ${children})
            if(IS_DIRECTORY ${BASE_DIR}/${child})
                create_source_groups(${BASE_DIR}/${child} ${REMOVE_PREFIX} ${BASE_GROUP})
            else()
                string(REPLACE "/" "\\" groupname ${BASE_DIR})
                string(REPLACE "${REMOVE_PREFIX}" "" groupname)
                if(NOT "${BASE_GROUP}" STREQUAL "")
                    set(groupname "${BASE_GROUP}\\${groupname}")
                endif()
                source_group(${groupname} FILES ${BASE_DIR}/${child})
            endif()
        endforeach()
    endfunction()
endif()
