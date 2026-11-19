cmake_minimum_required(VERSION 4.2)

if(NOT GSE_SDK_DIR)
    message(FATAL_ERROR "Pass -DGSE_SDK_DIR=<path to a packaged engine SDK image>.")
endif()

include("${GSE_SDK_DIR}/Engine/cmake/GSEEngineSdk.cmake")

project(SdkConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

gse_configure_compiler()
gse_sdk_link_modules()

add_executable(SdkConsumer Source/main.cpp)
gse_sdk_link_engine(SdkConsumer)
gse_sdk_write_manifest()
gse_copy_runtime_deps(SdkConsumer)
