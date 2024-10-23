cmake_minimum_required(VERSION 3.16)

# Set project name
project(TopLevelProject)

# Set the C++ standard for all subprojects
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories for the engine and game
add_subdirectory(engine)
add_subdirectory(game)

# Add the main executable that links the engine and game together
add_executable(${PROJECT_NAME} main.cpp)

# Link the engine and game libraries to the final executable
target_link_libraries(${PROJECT_NAME} PRIVATE Engine GoonSquad)

# MSVC options
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
endif()