cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(GoonSquadProject)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/*.cpp")
file(GLOB_RECURSE GAME_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/*.ixx")

# GoonSquadLib: Add as a static library
add_library(GoonSquadLib)

target_sources(GoonSquadLib
    PRIVATE
        ${GAME_SOURCES}
)

target_sources(GoonSquadLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Include"
        FILES ${GAME_MODULES}
)

# Include directories for GoonSquadLib
target_include_directories(GoonSquadLib PUBLIC 
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Game/Include"
)

# Link against Engine
target_link_libraries(GoonSquadLib PRIVATE Engine)

# GoonSquad: Add as an executable, without sources
add_executable(GoonSquad ${GAME_SOURCES})

# Resource path definition
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(GOONSQUAD_RESOURCES_PATH "./Game/Resources/")
else()
    set(GOONSQUAD_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
endif()

# Link against GoonSquadLib and Engine
target_link_libraries(GoonSquad PRIVATE GoonSquadLib Engine)

# Configure the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Game/Include/Config.ixx.in
    ${CMAKE_CURRENT_BINARY_DIR}/Game/Include/Config.ixx
    @ONLY
)
target_sources(GoonSquadLib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Game/Include/Config.ixx")

# MSVC-specific compile options
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

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
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Game/Source" "Source" "Game\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Game/Include" "Include" "Game\\Include")
endif()
