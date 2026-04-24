cmake_minimum_required(VERSION 3.16)

# Set C++ standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Editor)

# Gather Editor source files
file(GLOB_RECURSE Editor_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Source/*.cpp")

# Gather Editor header files
file(GLOB_RECURSE Editor_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/*.h")

# Add the Editor as an executable
add_executable(Editor ${Editor_SOURCES} ${Editor_HEADERS})

# Include directories for Editor headers
target_include_directories(Editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/")

# Link against the Engine (the Engine handles all third-party libraries) and the GoonSquadLib
target_link_libraries(Editor PRIVATE Engine)
target_link_libraries(Editor PRIVATE GoonSquadLib)

# Define the resource path
set(EDITOR_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")

# Configure the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/ResourcePaths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Editor/Include/ResourcePaths.h
    @ONLY
)

# Include directories for Editor headers
target_include_directories(Editor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Editor/Include/"
    "${CMAKE_CURRENT_BINARY_DIR}/Editor/Include"
)

set_target_properties(GoonSquadLib PROPERTIES EXCLUDE_FROM_ALL TRUE)

