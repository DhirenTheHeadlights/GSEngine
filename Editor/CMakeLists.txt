cmake_minimum_required(VERSION 3.16)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Editor)

# Gather Editor source files
file(GLOB_RECURSE Editor_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Source/*.cpp")

# Gather Editor header files
file(GLOB_RECURSE Editor_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/*.h")

# Add the Editor as an executable
add_executable(Editor ${Editor_SOURCES} ${Editor_HEADERS})

# Include directories for Editor headers
target_include_directories(Editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/")

# Link against the Engine (the Engine handles all third-party libraries) and the GoonSquadLib
target_link_libraries(Editor PRIVATE Engine)
target_link_libraries(Editor PRIVATE GoonSquadLib)

# Compilation Arguments
target_compile_definitions(Editor PRIVATE IMGUI = 0)

# Define macros for resource path
target_compile_definitions(Editor PUBLIC RESOURCES_PATH = "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")

set_target_properties(GoonSquadLib PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Set MSVC-specific compile options
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP) # Multi-core compilation

    # Function to create source groups by full directory hierarchy and remove unnecessary prefix
    function(create_source_groups_by_dir root_dir base_group remove_prefix)
        file(GLOB_RECURSE files LIST_DIRECTORIES false "${root_dir}/*")

        foreach(file_path ${files})
            file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}" "${file_path}")
            get_filename_component(dir "${relative_path}" DIRECTORY)
            string(REPLACE "/" "\\" group_path "${dir}")
        
            # Remove the unnecessary prefix
            string(REPLACE "${remove_prefix}" "" group_path "${group_path}")

            # Add base group if provided
            if (NOT "${base_group}" STREQUAL "")
                set(group_path "${base_group}\\${group_path}")
            endif()

            source_group("${group_path}" FILES "${file_path}")
        endforeach()
    endfunction()

    # Call the function for source and header files
    # Removing "Engine/Include" from the beginning of the path
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Editor/Source" "Source" "Editor\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include" "Include" "Editor\\Include")
endif()
