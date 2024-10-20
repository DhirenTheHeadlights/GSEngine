cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set production build option
option(PRODUCTION_BUILD "Make this a production build" OFF)

# MSVC-specific settings
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
endif()

project(GoonSquad)

# Gather game source files
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

# Gather game header files
file(GLOB_RECURSE MY_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp")

# Define the game executable
add_executable(GoonSquad ${MY_SOURCES} ${MY_HEADERS})

# Set the C++ standard for this target
set_property(TARGET GoonSquad PROPERTY CXX_STANDARD 23)

# Define macros for resource paths based on the build type
if(PRODUCTION_BUILD)
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="./Game/Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(GoonSquad PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Game/Resources/" PRODUCTION_BUILD=0)
endif()

# Set MSVC-specific compile options
if(MSVC)
    # Group source files under "Source Files" in Visual Studio
    source_group("Source Files" FILES ${MY_SOURCES})

    # Group header files under "Header Files" in Visual Studio
    source_group("Header Files" FILES ${MY_HEADERS})

    # Function to create source groups by full directory heirarchy
    function(create_source_groups BASE_DIR)
		file(GLOB children RELATIVE ${BASE_DIR} ${BASE_DIR}/*)
		foreach(child ${children})
			if(IS_DIRECTORY ${BASE_DIR}/${child})
				create_source_groups(${BASE_DIR}/${child})
			else()
				string(REPLACE "/" "\\" groupname ${BASE_DIR})
				source_group(${groupname} FILES ${BASE_DIR}/${child})
			endif()
		endforeach()
    endfunction()

    target_compile_definitions(GoonSquad PUBLIC _CRT_SECURE_NO_WARNINGS)

    if(PRODUCTION_BUILD)
        set_target_properties(GoonSquad PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") # No console in production
    else()
        set_target_properties(GoonSquad PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE") # Console for debugging
    endif()
endif()

# Include directories for game headers
target_include_directories(GoonSquad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include/")

# Link against the engine (the engine handles all third-party libraries)
target_link_libraries(GoonSquad PRIVATE Engine)
