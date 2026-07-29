cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared with every out-of-tree project that consumes this engine from source.
include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/cmake/GSEEngine.cmake")

project(GSEngine)

gse_configure_compiler()
gse_write_manifest()

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)
