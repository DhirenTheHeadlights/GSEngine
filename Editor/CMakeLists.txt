cmake_minimum_required(VERSION 3.16)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Editor)

# Gather Editor source files
file(GLOB_RECURSE Editor_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

# Gather Editor header files
file(GLOB_RECURSE Editor_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h")

# Add the Editor as an executable
add_executable(Editor ${Editor_SOURCES} ${Editor_HEADERS})

# Include directories for Editor headers
target_include_directories(Editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include/")

# Link against the Engine (the Engine handles all third-party libraries) and the GoonSquadLib
target_link_libraries(Editor PRIVATE Engine)
target_link_libraries(Editor PRIVATE GoonSquadLib)

# Define macros for resource paths based on the build type
if(PRODUCTION_BUILD)
    target_compile_definitions(Editor PUBLIC RESOURCES_PATH="./Editor/Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(Editor PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Editor/Resources/" PRODUCTION_BUILD=0)
endif()

# Set MSVC-specific compile options
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP) # Multi-core compilation

    # Function to create source groups by full directory hierarchy and remove unnecessary prefix
    function(create_source_groups BASE_DIR REMOVE_PREFIX BASE_GROUP)
        file(GLOB_RECURSE children RELATIVE ${BASE_DIR} ${BASE_DIR}/*)

        foreach(child ${children})
            if(IS_DIRECTORY ${BASE_DIR}/${child})
                create_source_groups(${BASE_DIR}/${child} ${REMOVE_PREFIX} ${BASE_GROUP})
            else()
                string(REPLACE "/" "\\" groupname ${BASE_DIR})
            
                # Remove the unnecessary prefix
                string(REPLACE "${REMOVE_PREFIX}" "" groupname)

                # Add base group if provided
                if(NOT "${BASE_GROUP}" STREQUAL "")
                    set(groupname "${BASE_GROUP}\\${groupname}")
                endif()

                source_group(${groupname} FILES ${BASE_DIR}/${child})
            endif()
        endforeach()
    endfunction()
endif()
