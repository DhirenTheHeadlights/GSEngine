cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set production build option
option(PRODUCTION_BUILD "Make this a production build" OFF)

project(GoonSquad)

# Gather game source and header files
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h")

# Add the game as an executable
add_executable(GoonSquad ${GAME_SOURCES} ${GAME_HEADERS})

# Include directories for game headers
target_include_directories(GoonSquad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include/")

# Link against the engine
target_link_libraries(GoonSquad PRIVATE Engine)

# Define macros for resource paths based on the build type
if(PRODUCTION_BUILD)
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="./Game/Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Game/Resources/" PRODUCTION_BUILD=0)
endif()

# MSVC-specific compile options
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP) # Multi-core compilation

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
