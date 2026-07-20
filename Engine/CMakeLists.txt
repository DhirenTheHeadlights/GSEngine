cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine LANGUAGES CXX)

file(GLOB_RECURSE ENGINE_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cppm" "${CMAKE_CURRENT_BINARY_DIR}/Engine/*.cppm")
list(REMOVE_ITEM ENGINE_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Engine.cppm")

file(GLOB_RECURSE ENGINE_IMPLS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cpp")
list(REMOVE_ITEM ENGINE_IMPLS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/External/MiniaudioImpl.cpp")

# A .cpp that declares an implementation partition (module gse.x:y;) provides a
# module and must live in a CXX_MODULES file set; a plain implementation unit
# (module gse.x;) or global-module .cpp stays an ordinary source.
set(ENGINE_MODULE_IMPLS "")
set(ENGINE_PLAIN_IMPLS "")
foreach(_impl IN LISTS ENGINE_IMPLS)
    file(STRINGS "${_impl}" _part_decl REGEX "^module gse\\.[A-Za-z_0-9]+:[A-Za-z_0-9]+ *;" LIMIT_COUNT 1)
    if(_part_decl)
        list(APPEND ENGINE_MODULE_IMPLS "${_impl}")
    else()
        list(APPEND ENGINE_PLAIN_IMPLS "${_impl}")
    endif()
endforeach()

add_library(Engine)

target_sources(Engine
    PUBLIC
        FILE_SET gse_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine" "${CMAKE_CURRENT_BINARY_DIR}/Engine"
        FILES ${ENGINE_MODULES}
)

target_sources(Engine
    PRIVATE
        FILE_SET gse_module_impls TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine" "${CMAKE_CURRENT_BINARY_DIR}/Engine"
        FILES ${ENGINE_MODULE_IMPLS}
)

if(ENGINE_PLAIN_IMPLS)
    target_sources(Engine PRIVATE ${ENGINE_PLAIN_IMPLS})
endif()

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
find_package(Vulkan)
find_package(unofficial-nsight-aftermath CONFIG QUIET)

add_library(VulkanModule STATIC)
target_sources(VulkanModule PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${Vulkan_INCLUDE_DIR}
    FILES
        ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
        ${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm)
target_compile_features(VulkanModule PUBLIC cxx_std_26)
target_link_libraries(VulkanModule PUBLIC Vulkan::Headers)
target_compile_definitions(VulkanModule PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC VULKAN_HPP_NO_CONSTRUCTORS VULKAN_HPP_NO_EXCEPTIONS)

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
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(Engine PUBLIC stdc++exp)
    if(WIN32)
        find_package(directx-headers CONFIG REQUIRED)
        target_link_libraries(Engine PUBLIC ws2_32 dbghelp d3d12 dxgi dxguid Microsoft::DirectX-Headers)
    endif()
endif()

if(TARGET unofficial::nsight-aftermath::nsight-aftermath)
    target_link_libraries(Engine PRIVATE unofficial::nsight-aftermath::nsight-aftermath)
    target_compile_definitions(Engine PRIVATE GSE_HAVE_AFTERMATH=1)
    message(STATUS "Nsight Aftermath SDK found; GPU crash dumps enabled")
else()
    message(STATUS "Nsight Aftermath SDK not found; GPU crash dumps disabled (enable with vcpkg feature 'aftermath' and NSIGHT_AFTERMATH_SDK env var)")
endif()

# DirectX 12 Agility SDK: fetch a pinned D3D12 runtime so mesh shaders / SM6.6 bindless behave
# the same on every machine instead of depending on the OS-shipped runtime. The DX12 backend owns
# the loader exports (Engine/Source/Dx12/AgilitySdk.cpp); they're force-linked into any exe that
# links Engine, so application code never touches D3D12. gse_copy_runtime_deps() copies the DLLs
# into the exe's D3D12/ subfolder. Bump GSE_AGILITY_VERSION (+ GSE_AGILITY_SDK_NUMBER) together.
if(WIN32)
    set(GSE_AGILITY_VERSION "1.616.0")
    set(GSE_AGILITY_SDK_NUMBER 616)
    set(_agility_root "${CMAKE_BINARY_DIR}/_agility/${GSE_AGILITY_VERSION}")
    set(_agility_bin "${_agility_root}/build/native/bin/x64")
    if(NOT EXISTS "${_agility_bin}/D3D12Core.dll")
        set(_agility_zip "${CMAKE_BINARY_DIR}/_agility/agility-${GSE_AGILITY_VERSION}.zip")
        message(STATUS "Fetching DirectX 12 Agility SDK ${GSE_AGILITY_VERSION} ...")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${GSE_AGILITY_VERSION}"
            "${_agility_zip}"
            STATUS _agility_status
            TLS_VERIFY ON
        )
        list(GET _agility_status 0 _agility_code)
        if(_agility_code EQUAL 0)
            file(ARCHIVE_EXTRACT INPUT "${_agility_zip}" DESTINATION "${_agility_root}")
        else()
            list(GET _agility_status 1 _agility_msg)
            message(WARNING "Agility SDK download failed (${_agility_msg}); falling back to the OS D3D12 runtime.")
        endif()
    endif()
    if(EXISTS "${_agility_bin}/D3D12Core.dll")
        set(GSE_AGILITY_BIN_DIR "${_agility_bin}" CACHE INTERNAL "DirectX 12 Agility SDK x64 runtime dir")
        target_compile_definitions(Engine PRIVATE GSE_D3D12_AGILITY_VERSION=${GSE_AGILITY_SDK_NUMBER})
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(Engine INTERFACE "-Wl,--undefined=D3D12SDKVersion" "-Wl,--undefined=D3D12SDKPath")
        else()
            target_link_options(Engine INTERFACE "/INCLUDE:D3D12SDKVersion" "/INCLUDE:D3D12SDKPath")
        endif()
    endif()
endif()

set(GSE_ROOT_DIR "${CMAKE_SOURCE_DIR}")
set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/")
set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
set(BAKED_RESOURCES_PATH "${CMAKE_CURRENT_BINARY_DIR}/Resources/")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Import/Config.cppm.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Import/Config.cppm
    @ONLY
)

