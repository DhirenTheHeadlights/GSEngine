cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine LANGUAGES CXX)

file(GLOB_RECURSE ENGINE_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cppm" "${CMAKE_CURRENT_BINARY_DIR}/Engine/*.cppm")
list(REMOVE_ITEM ENGINE_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Engine.cppm")

file(GLOB_RECURSE ENGINE_IMPLS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cpp")
list(REMOVE_ITEM ENGINE_IMPLS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/External/MiniaudioImpl.cpp")

add_library(Engine)

target_sources(Engine
    PUBLIC
        FILE_SET gse_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine" "${CMAKE_CURRENT_BINARY_DIR}/Engine"
        FILES ${ENGINE_MODULES}
)

target_sources(Engine PRIVATE ${ENGINE_IMPLS})

target_sources(Engine
    PUBLIC
        FILE_SET gse_modules_public TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Engine.cppm"
)

find_package(glfw3 CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_path(miniaudio_INCLUDE_DIR "miniaudio.h")
find_package(Freetype CONFIG REQUIRED)
find_package(msdfgen CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(Vulkan)

add_library(VulkanModule STATIC)
target_sources(VulkanModule PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${Vulkan_INCLUDE_DIR}
    FILES
        ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
        ${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm)
target_compile_features(VulkanModule PUBLIC cxx_std_26)
target_link_libraries(VulkanModule PUBLIC Vulkan::Headers)
target_compile_definitions(VulkanModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC VULKAN_HPP_NO_CONSTRUCTORS)

add_library(MiniaudioImpl STATIC "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/External/MiniaudioImpl.cpp")
target_include_directories(MiniaudioImpl PRIVATE ${miniaudio_INCLUDE_DIR})

set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})

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

target_include_directories(Engine
    PUBLIC
        ${miniaudio_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/External
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
    VulkanModule
    PNG::PNG
    JPEG::JPEG
    MiniaudioImpl
    concurrentqueue::concurrentqueue
)

set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(BAKED_RESOURCES_PATH "${CMAKE_CURRENT_BINARY_DIR}/Resources/")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Config.cppm.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Import/Config.cppm
    @ONLY
)
