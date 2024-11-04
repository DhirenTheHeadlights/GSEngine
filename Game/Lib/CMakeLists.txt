cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(GoonSquadLib)

# Gather game source and header files for the shared library
file(GLOB_RECURSE GAME_LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Source/*.cpp")
file(GLOB_RECURSE GAME_LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Include/*.h")

# Add the game logic as a shared library
add_library(GoonSquadLib STATIC ${GAME_LIB_SOURCES} ${GAME_LIB_HEADERS})

# Include directories for the game library headers
target_include_directories(GoonSquadLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Include/")

# Link against the Engine
target_link_libraries(GoonSquadLib PRIVATE Engine)

# Define macros for resource paths
if(PRODUCTION_BUILD)
    target_compile_definitions(GoonSquadLib PUBLIC RESOURCES_PATH="./Game/Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(GoonSquadLib PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../Resources/" PRODUCTION_BUILD=0)
endif()
