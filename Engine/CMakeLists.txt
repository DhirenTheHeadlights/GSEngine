cmake_minimum_required(VERSION 3.26)

# Set project details and C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

# Define the base directory for FetchContent dependencies
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/Engine/External" CACHE STRING "Base directory for FetchContent dependencies" FORCE)

# Ensure the FetchContent base directory exists
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")

# Include FetchContent module
include(FetchContent)

# Gather engine source files
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/*.cpp")
file(GLOB_RECURSE ENGINE_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/*.ixx")

add_library(Engine)

target_sources(Engine
    PRIVATE
        ${ENGINE_SOURCES}
)

target_sources(Engine
    PRIVATE
        FILE_SET gse_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include"
        FILES ${ENGINE_MODULES}
)

target_sources(Engine
    PUBLIC
        FILE_SET gse_modules_public TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/Engine.ixx"
)

# Option for production build
option(PRODUCTION_BUILD "Make this a production build" OFF)

# Define the resource path based on build type
if(PRODUCTION_BUILD)
    set(ENGINE_RESOURCES_PATH "./Resources/")
else()
    set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
endif()

# Configure the header file with the resource path
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/Core/ResourcePaths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Include/Core/ResourcePaths.h
    @ONLY
)

# Add the binary directory to include paths so that generated headers are found
target_include_directories(Engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include"
    "${CMAKE_CURRENT_BINARY_DIR}/Engine/Include"
)

####################################################################################################################
# FetchContent for glfw
####################################################################################################################

set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW: Build documentation" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW: Build examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW: Build tests" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

FetchContent_MakeAvailable(glfw)

# Include GLFW headers
target_include_directories(Engine PUBLIC ${glfw_SOURCE_DIR}/include)

####################################################################################################################
# FetchContent for glad
####################################################################################################################

option(GLAD_GENERATE "Generate GLAD source files" OFF)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

FetchContent_MakeAvailable(glad)

# Include GLAD headers
target_include_directories(Engine PUBLIC ${glad_SOURCE_DIR}/include)

####################################################################################################################
# FetchContent for stb_image and stb_truetype
####################################################################################################################

# stb_image
FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master 
)

FetchContent_MakeAvailable(stb_image)

# Include stb_image headers
target_include_directories(Engine PUBLIC ${stb_image_SOURCE_DIR})

####################################################################################################################
# FetchContent for enet
####################################################################################################################

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.17
)

FetchContent_MakeAvailable(enet)

# Include ENet headers
target_include_directories(Engine PUBLIC ${enet_SOURCE_DIR}/include)

####################################################################################################################
# FetchContent for miniaudio
####################################################################################################################

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(miniaudio)

# Include miniaudio headers
target_include_directories(Engine PUBLIC ${miniaudio_SOURCE_DIR})

####################################################################################################################
# FetchContent for GLM
####################################################################################################################

set(GLM_ENABLE_CXX_20 ON CACHE BOOL "Enable C++20 features in GLM" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(glm)

add_library(glm_module STATIC)
target_sources(glm_module PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS "${glm_SOURCE_DIR}/glm"
    FILES "${glm_SOURCE_DIR}/glm/glm.cppm"
)
target_link_libraries(Engine PUBLIC glm_module)
target_link_libraries(glm_module PUBLIC glm)

####################################################################################################################
# Grab freetype & msdfgen from the vcpkg registry
####################################################################################################################

find_package(Freetype CONFIG REQUIRED)
find_package(msdfgen CONFIG REQUIRED)

####################################################################################################################

# Add subdirectory for imgui-docking
add_subdirectory(External/imgui-docking)

# Include directories for engine headers
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/"
)

# Include directories for third-party libraries (e.g., nlohmann)
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/External/nlohmann"
)

# Link the third-party libraries to the engine
target_link_libraries(Engine PUBLIC
    glfw
    glad
    imgui
    enet
    glm
    Freetype::Freetype
    msdfgen::msdfgen
)

# Visual Studio specific settings
if (MSVC)
    # Configure the MSVC runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Add compiler options for SIMD optimizations and multi-core compilation
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

    # We have to inject cpp 23 module build support using a property file to work with the current CMake version
    SET_PROPERTY(TARGET Engine PROPERTY VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Resources/Misc/msvc_build_stl_modules_workaround.props")

    # List of external targets to be grouped under "External"
    set(EXTERNAL_TARGETS enet glad glad-generate-files glm imgui glm_module)

    # Loop through each target and set its folder property
    foreach(target ${EXTERNAL_TARGETS})
        set_property(TARGET ${target} PROPERTY FOLDER "External")
    endforeach()

    # Move glfw-related projects under External/GLFW3

    # Collect targets from the glfw root directory
    get_property(ROOT_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)

    # Collect targets from the glfw 'src' directory
    get_property(SRC_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}/src" PROPERTY BUILDSYSTEM_TARGETS)

    # Combine both sets of subprojects into one list
    set(GLFW_SUBPROJECTS ${ROOT_SUBPROJECTS} ${SRC_SUBPROJECTS})

    # Loop through the combined list of subprojects
    foreach(subproject ${GLFW_SUBPROJECTS})
        set_property(TARGET ${subproject} PROPERTY FOLDER "External/GLFW3")
    endforeach()

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
    # Removing "Engine/Source" and "Engine/Include" from the beginning of the path
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source" "Source" "Engine\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include" "Include" "Engine\\Include")

    # Manually include glm headers in a visible group
    file(GLOB_RECURSE GLM_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/External/glm/glm/*.hpp")
    source_group("External\\glm" FILES ${GLM_HEADERS})
endif()

# Suppress Winsock deprecation warnings globally for the Engine target
target_compile_definitions(Engine PUBLIC _WINSOCK_DEPRECATED_NO_WARNINGS)