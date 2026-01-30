cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

file(GLOB_RECURSE ENGINE_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cppm" "${CMAKE_CURRENT_BINARY_DIR}/Engine/*.cppm")

add_library(Engine)

target_sources(Engine
    PRIVATE
        ${ENGINE_MODULES}
)

target_sources(Engine
    PRIVATE
        FILE_SET gse_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine" "${CMAKE_CURRENT_BINARY_DIR}/Engine"
        FILES ${ENGINE_MODULES}
)

target_sources(Engine
    PUBLIC
        FILE_SET gse_modules_public TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Engine.cppm"
)

find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(miniaudio_INCLUDE_DIR "miniaudio.h")
find_package(Freetype CONFIG REQUIRED)
find_package(msdfgen CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
if(NOT VULKAN_SDK_PATH)
    message(FATAL_ERROR "Vulkan SDK not found! Make sure the VULKAN_SDK environment variable is set.")
endif()

set(HAVE_SLANG_PACKAGE OFF)
find_package(slang CONFIG QUIET)
if(slang_FOUND)
    set(HAVE_SLANG_PACKAGE ON)
else()
    find_path(SLANG_INCLUDE_DIR
        NAMES slang.h
        HINTS "${VULKAN_SDK_PATH}/Include" "${VULKAN_SDK_PATH}/include"
    )
    find_library(SLANG_LIBRARY
        NAMES slang slang.dll libslang.so libslang.dylib
        HINTS "${VULKAN_SDK_PATH}/Lib" "${VULKAN_SDK_PATH}/lib"
    )
    if(NOT SLANG_INCLUDE_DIR OR NOT SLANG_LIBRARY)
        message(FATAL_ERROR "Slang not found (neither package nor SDK). Install via vcpkg (slang) or use Vulkan SDK >= 1.3.296.")
    endif()
endif()

add_library(VulkanModule)
target_compile_features(VulkanModule PUBLIC cxx_std_23)
target_compile_definitions(VulkanModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC VULKAN_HPP_NO_CONSTRUCTORS)
target_link_libraries(VulkanModule PUBLIC Vulkan::Vulkan)
target_sources(VulkanModule PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm ${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm
)

target_include_directories(Engine
    PUBLIC
        ${Stb_INCLUDE_DIR}
        ${miniaudio_INCLUDE_DIR}
)

if(HAVE_SLANG_PACKAGE)
    target_link_libraries(Engine PUBLIC slang::slang)
else()
    target_include_directories(Engine PRIVATE "${SLANG_INCLUDE_DIR}")
    target_link_libraries(Engine PUBLIC "${SLANG_LIBRARY}")
endif()

target_link_libraries(Engine PUBLIC
    glfw
    Freetype::Freetype
    msdfgen::msdfgen
    tomlplusplus::tomlplusplus
    VulkanModule
    TBB::tbb
)

set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(BAKED_RESOURCES_PATH "${CMAKE_CURRENT_BINARY_DIR}/Resources/")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/Core/Utility/Config.cppm.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Source/Core/Utility/Config.cppm
    @ONLY
)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_compile_options(/arch:AVX2)
    add_compile_options(/MP)
    add_compile_options(/Zm500)

    set(EXTERNAL_TARGETS VulkanModule)
    foreach(target ${EXTERNAL_TARGETS})
        set_property(TARGET ${target} PROPERTY FOLDER "External")
    endforeach()
endif()
