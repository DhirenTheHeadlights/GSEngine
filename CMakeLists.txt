cmake_minimum_required(VERSION 3.16)

# Set the C++ standard for all subprojects
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories for the engine and game
add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Editor)