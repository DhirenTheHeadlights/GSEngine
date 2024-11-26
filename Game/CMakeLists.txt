cmake_minimum_required(VERSION 3.27)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the project
project(GoonSquadProject)

# Option for production build
option(PRODUCTION_BUILD "Make this a production build" OFF)

# Include directories
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Game/Include")

# Gather game source files
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Source/*.cpp")

# Gather module interface and implementation files
file(GLOB MODULE_FILES CONFIGURE_DEPENDS "${INCLUDE_DIR}/*.cppm" "${INCLUDE_DIR}/*.ixx")
file(GLOB MODULE_IMPL_FILES CONFIGURE_DEPENDS "${INCLUDE_DIR}/*.cpp")

# Add GoonSquadLib as a static library
add_library(GoonSquadLib STATIC ${GAME_SOURCES} ${MODULE_FILES} ${MODULE_IMPL_FILES})

# Add modules
target_sources(GoonSquadLib PRIVATE ${MODULE_FILES} ${MODULE_IMPL_FILES})

# Include directories for GoonSquadLib
target_include_directories(GoonSquadLib PUBLIC 
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Game/Include"
)

# Link against the Engine
target_link_libraries(GoonSquadLib PRIVATE Engine)

# Add GoonSquad as an executable
add_executable(GoonSquad ${GAME_SOURCES})

# Set resource path
if(PRODUCTION_BUILD)
    set(GOONSQUAD_RESOURCES_PATH "./Game/Resources/")
else()
    set(GOONSQUAD_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
endif()

# Link GoonSquad against GoonSquadLib and Engine
target_link_libraries(GoonSquad PRIVATE GoonSquadLib Engine)

# Configure the resource paths header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/Include/ResourcePaths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Game/Include/ResourcePaths.h
    @ONLY
)

# Windows-specific settings for Visual Studio
if(MSVC)
    # Set MSVC runtime library and enable AVX2 optimizations
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

    # Function to organize source groups by directory
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
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Game/Source" "Source" "Game\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Game/Include" "Include" "Game\\Include")
endif()
