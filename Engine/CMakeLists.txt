cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

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

target_sources(Engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Engine/Source/Core/Config.ixx")

####################################################################################################################
# Grab dependencies from vcpkg
####################################################################################################################

find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(miniaudio_INCLUDE_DIR "miniaudio.h")
find_package(Freetype CONFIG REQUIRED)
find_package(msdfgen CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# Set up Vulkan C++ module as a library
add_library(Vulkan)

target_sources(Vulkan PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
)

target_compile_features(Vulkan PUBLIC cxx_std_20)
target_link_libraries(Vulkan PUBLIC Vulkan::Headers)

target_compile_definitions(Vulkan PUBLIC
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
)

set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})

if(NOT VULKAN_SDK_PATH)
    message(FATAL_ERROR "Vulkan SDK not found! Make sure the VULKAN_SDK environment variable is set.")
endif()

target_include_directories(Vulkan PUBLIC
    "${VULKAN_SDK_PATH}/Include"
)

####################################################################################################################

add_subdirectory(External/imgui-docking)

target_include_directories(Engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/External/imgui-docking"
        "${CMAKE_CURRENT_SOURCE_DIR}/External/nlohmann"
        ${Stb_INCLUDE_DIR},
        ${miniaudio_INCLUDE_DIR}
)

target_link_libraries(Engine PUBLIC
    glfw
    imgui
    Freetype::Freetype
    msdfgen::msdfgen
    Vulkan
)

####################################################################################################################
# Compile shader files
####################################################################################################################

find_program(GLSLC glslc)
if(NOT GLSLC)
  message(FATAL_ERROR "glslc not found! Please install the Vulkan SDK and ensure glslc is in your PATH.")
endif()

file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/*.frag")

set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(SPIRV "")
set(SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/Resources/Shaders")
set(ADDED_SHADERS "")

foreach(shader ${SHADER_FILES})
    get_filename_component(shader_name ${shader} NAME_WE)
    get_filename_component(shader_ext ${shader} EXT) # Get extension (.vert, .frag, etc.)
    set(shader_output "${SHADER_DIR}/${shader_name}${shader_ext}.spv")

    list(FIND ADDED_SHADERS ${shader_output} shader_exists)
    if(shader_exists EQUAL -1)
        add_custom_command(
            OUTPUT ${shader_output}
            COMMAND ${GLSLC} ${shader} -o ${shader_output}
            DEPENDS ${shader}
        )
        list(APPEND SPIRV ${shader_output})
        list(APPEND ADDED_SHADERS ${shader_output})
    endif()
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV})
add_dependencies(Engine Shaders)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/Core/Config.ixx.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Source/Core/Config.ixx
    @ONLY
)

if (MSVC)
    # Configure the MSVC runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Add compiler options for SIMD optimizations and multi-core compilation
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

    # List of external targets to be grouped under "External"
    set(EXTERNAL_TARGETS imgui Vulkan)

    # Loop through each target and set its folder property
    foreach(target ${EXTERNAL_TARGETS})
        set_property(TARGET ${target} PROPERTY FOLDER "External")
    endforeach()
endif()