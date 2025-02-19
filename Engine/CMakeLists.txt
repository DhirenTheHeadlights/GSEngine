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
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine" 
        FILES ${ENGINE_MODULES} ${ENGINE_SOURCES}
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

# Configure a module file with the resource path
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/Core/Config.ixx.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Source/Core/Config.ixx
    @ONLY
)
target_sources(Engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Engine/Source/Core/Config.ixx")

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
# Grab freetype, msdfgen, and Vulkan from the vcpkg registry
####################################################################################################################

find_package(Freetype CONFIG REQUIRED)
find_package(msdfgen CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# Set up Vulkan C++ module as a library
add_library(VulkanHppModule)

target_sources(VulkanHppModule PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
)

target_compile_features(VulkanHppModule PUBLIC cxx_std_20)
target_link_libraries(VulkanHppModule PUBLIC Vulkan::Headers)

target_compile_definitions(VulkanHppModule PUBLIC
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC = 1
)

####################################################################################################################

# Add subdirectory for imgui-docking
add_subdirectory(External/imgui-docking)

# Include directories for third-party libraries
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/External/nlohmann"
)

# Link the third-party libraries to the engine
target_link_libraries(Engine PUBLIC
    glfw
    glad
    imgui
    Freetype::Freetype
    msdfgen::msdfgen
    VulkanHppModule
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
    set(EXTERNAL_TARGETS glad glad-generate-files imgui)

    # Loop through each target and set its folder property
    foreach(target ${EXTERNAL_TARGETS})
        set_property(TARGET ${target} PROPERTY FOLDER "External")
    endforeach()

    # Move glfw-related projects under External/GLFW3
    get_property(ROOT_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(SRC_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}/src" PROPERTY BUILDSYSTEM_TARGETS)
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
endif()