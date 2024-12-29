cmake_minimum_required(VERSION 3.16)

# Set project details and C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

# Define the base directory for FetchContent dependencies
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/Engine/External" CACHE STRING "Base directory for FetchContent dependencies" FORCE)

# Ensure the FetchContent base directory exists
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")

# Include FetchContent module
include(FetchContent)

# Gather engine source files (cpp) from the "Source" folder
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/*.cpp")

# Gather engine header files (h, hpp) from the "Include" folder
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/*.h")

# Add the engine as a static library including source and header files
add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Option for production build
option(PRODUCTION_BUILD "Make this a production build" OFF)

# Define the resource path based on build type
if(PRODUCTION_BUILD)
    set(ENGINE_RESOURCES_PATH "./Resources/")
else()
    set(ENGINE_RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Resources/")
endif()

# Configure the header file with the resource path
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/Core/ResourcePaths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Engine/Include/Core/ResourcePaths.h
    @ONLY
)

# Add the binary directory to include paths so that generated headers are found
target_include_directories(Engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include"
    "${CMAKE_CURRENT_BINARY_DIR}/Engine/Include"
)

##############################################
# FetchContent for glfw
##############################################

set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW: Build documentation" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW: Build examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW: Build tests" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

FetchContent_MakeAvailable(glfw)

# Include GLFW headers
target_include_directories(Engine PUBLIC ${glfw_SOURCE_DIR}/include)

##############################################

##############################################
# FetchContent for glad
##############################################

option(GLAD_GENERATE "Generate GLAD source files" OFF)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

FetchContent_MakeAvailable(glad)

# Include GLAD headers
target_include_directories(Engine PUBLIC ${glad_SOURCE_DIR}/include)

############################################

##############################################
# FetchContent for stb_image and stb_truetype
##############################################

# stb_image
FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master 
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_MakeAvailable(stb_image)

# Include stb_image headers
target_include_directories(Engine PUBLIC ${stb_image_SOURCE_DIR})

# stb_truetype (from the same stb repository)
FetchContent_Declare(
    stb_truetype
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_MakeAvailable(stb_truetype)

# Include stb_truetype headers
target_include_directories(Engine PUBLIC ${stb_truetype_SOURCE_DIR})

############################################

##############################################
# FetchContent for enet
##############################################

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.17
)

FetchContent_MakeAvailable(enet)

# Include ENet headers
target_include_directories(Engine PUBLIC ${enet_SOURCE_DIR}/include)

############################################

##############################################
# FetchContent for miniaudio
##############################################

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(miniaudio)

# Include miniaudio headers
target_include_directories(Engine PUBLIC ${miniaudio_SOURCE_DIR})

############################################

##############################################
# FetchContent for GLM
##############################################

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(glm)

############################################

##############################################
# FetchContent for zlib
##############################################

# Disable pkg-config to avoid missing PkgConfig errors on Windows
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)

# Skip installing steps for zlib
set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip all install steps for zlib" FORCE)
set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "Skip installing zlib libraries" FORCE)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.2.13
)

FetchContent_MakeAvailable(zlib)

# Force zlib variables to ensure libpng can find zlib
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
set(ZLIB_FOUND ON CACHE BOOL "" FORCE)

# Link zlib's include directories properly
target_include_directories(zlibstatic INTERFACE 
    "${zlib_SOURCE_DIR}"
    "${zlib_BINARY_DIR}"
)

##############################################

##############################################
# FetchContent for libpng
##############################################

# Configure libpng to build only static library without tests and executables
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(PNG_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    libpng
    GIT_REPOSITORY https://github.com/glennrp/libpng.git
    GIT_TAG        v1.6.40
)

FetchContent_MakeAvailable(libpng)

# Link libpng to zlib
target_link_libraries(png_static zlibstatic)

# Include libpng's include directories
target_include_directories(Engine PUBLIC
    "${libpng_SOURCE_DIR}"
    "${libpng_BINARY_DIR}"
)

##############################################

##############################################
# FetchContent for freetype and minimal dependencies
##############################################

# Configure FreeType options
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB OFF CACHE BOOL "" FORCE)   # Keep zlib for FreeType compression
set(FT_DISABLE_PNG OFF CACHE BOOL "" FORCE)    # Keep PNG support if msdfgen uses it
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_ENABLE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(FT_SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        master  
)

FetchContent_MakeAvailable(freetype)

# Include FreeType and related headers
target_include_directories(Engine PUBLIC
    "${freetype_SOURCE_DIR}/include"
    "${zlib_SOURCE_DIR}"           # For zlib.h
    "${zlib_BINARY_DIR}"           # For zconf.h
)

if (TARGET freetype AND TARGET png_static)
    # Enable FreeType usage in msdfgen
    set(MSDFGEN_USE_FREETYPE ON CACHE BOOL "" FORCE)

    # Specify FreeType include directories
    set(FREETYPE_INCLUDE_DIRS 
        "${freetype_SOURCE_DIR}/include" 
        "${freetype_BINARY_DIR}/include" 
        CACHE PATH "" FORCE
    )

    # Set Freetype library paths based on build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FREETYPE_LIBRARY "${freetype_BINARY_DIR}/freetyped.lib" CACHE STRING "" FORCE)
    else()
        set(FREETYPE_LIBRARY "${freetype_BINARY_DIR}/freetype.lib" CACHE STRING "" FORCE)
    endif()

    # Specify libpng include directories
    set(PNG_PNG_INCLUDE_DIR 
        "${libpng_SOURCE_DIR}" 
        CACHE PATH "" FORCE
    )

    # Specify libpng library
    set(PNG_LIBRARY png_static CACHE STRING "" FORCE)

    target_include_directories(freetype PRIVATE
        "${libpng_SOURCE_DIR}"
        "${libpng_BINARY_DIR}"
    )
endif()

add_library(PNG::PNG ALIAS png_static)

# **Set FindPNG Variables Manually**
set(PNG_INCLUDE_DIR "${libpng_SOURCE_DIR}" CACHE PATH "Path to libpng include directory" FORCE)
set(PNG_LIBRARIES png_static CACHE STRING "libpng libraries" FORCE)
set(PNG_LIBRARY png_static CACHE STRING "libpng library" FORCE)
set(PNG_FOUND TRUE CACHE BOOL "libpng found" FORCE)

##############################################

##############################################
# FetchContent for msdfgen
##############################################

# FetchContent for tinyxml2
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG        9.0.0  # or whichever release/tag you want
)

FetchContent_MakeAvailable(tinyxml2)

# Set variables for msdfgen to locate libpng
set(PNG_PNG_INCLUDE_DIR "${libpng_SOURCE_DIR}" CACHE PATH "" FORCE)
set(PNG_LIBRARY png_static CACHE STRING "" FORCE)
set(PNG_LIBRARY_DIR "${libpng_BINARY_DIR}" CACHE PATH "" FORCE)

# Configure msdfgen options
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG FALSE CACHE BOOL "" FORCE)
set(MSDFGEN_USE_TINYXML2 ON CACHE BOOL "" FORCE)
set(MSDFGEN_USE_FREETYPE ON CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  msdfgen
  GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
  GIT_TAG master
)

FetchContent_MakeAvailable(msdfgen)

# Set required configuration variables before generating msdfgen-config.h
if(BUILD_SHARED_LIBS AND WIN32)
    set(MSDFGEN_PUBLIC_MACRO_VALUE " __declspec(dllexport)")
    set(MSDFGEN_EXT_PUBLIC_MACRO_VALUE " __declspec(dllexport)")
else()
    set(MSDFGEN_PUBLIC_MACRO_VALUE "")
    set(MSDFGEN_EXT_PUBLIC_MACRO_VALUE "")
endif()

# Set version information (adjust as needed)
set(MSDFGEN_VERSION "1.0.0")
set(MSDFGEN_VERSION_MAJOR "1")
set(MSDFGEN_VERSION_MINOR "0")
set(MSDFGEN_VERSION_REVISION "0")
set(MSDFGEN_COPYRIGHT_YEAR "2023")

# Set additional defines based on configuration options
set(MSDFGEN_ADDITIONAL_DEFINES "")
if(MSDFGEN_USE_CPP11)
    set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_USE_CPP11")
endif()
if(MSDFGEN_USE_OPENMP)
    set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_USE_OPENMP")
endif()
if(NOT MSDFGEN_CORE_ONLY)
    set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_EXTENSIONS")
    if(MSDFGEN_USE_SKIA)
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_USE_SKIA")
    endif()
    if(NOT MSDFGEN_DISABLE_SVG)
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_USE_TINYXML2")
    else()
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_DISABLE_SVG")
    endif()
    if(NOT MSDFGEN_DISABLE_PNG)
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_USE_LIBPNG")
    else()
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_DISABLE_PNG")
    endif()
    if(MSDFGEN_DISABLE_VARIABLE_FONTS)
        set(MSDFGEN_ADDITIONAL_DEFINES "${MSDFGEN_ADDITIONAL_DEFINES}\n#define MSDFGEN_DISABLE_VARIABLE_FONTS")
    endif()
endif()

# Create the msdfgen subdirectory in the build directory
file(MAKE_DIRECTORY "${msdfgen_BINARY_DIR}/msdfgen")

# Manually generate msdfgen-config.h based on msdfgen's configuration
configure_file(
    "${msdfgen_SOURCE_DIR}/cmake/msdfgen-config.h.in"
    "${msdfgen_BINARY_DIR}/msdfgen/msdfgen-config.h"
)

# Include msdfgen's generated config and source include directories
target_include_directories(Engine PUBLIC 
    "${msdfgen_SOURCE_DIR}" 
    "${msdfgen_BINARY_DIR}"
)

target_include_directories(msdfgen-ext PRIVATE
    "${libpng_SOURCE_DIR}"
    "${libpng_BINARY_DIR}"
)

##############################################

# Add subdirectory for imgui-docking (assuming it's correctly set up)
add_subdirectory(External/imgui-docking)

# Include directories for engine headers
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/"
)

# Include directories for third-party libraries (e.g., nlohmann)
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/External/nlohmann"
)

# Link the third-party libraries to the engine
target_link_libraries(Engine PUBLIC
    glfw
    glad
    imgui
    enet
    glm
    zlibstatic
    png_static
    "${FREETYPE_LIBRARY}"
    tinyxml2
    msdfgen::msdfgen
)

# Visual Studio specific settings
if (MSVC)
    # Configure the MSVC runtime library
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Add compiler options for SIMD optimizations and multi-core compilation
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)        # Multi-core compilation

    # List of external targets to be grouped under "External"
    set(EXTERNAL_TARGETS glfw glad glad-generate-files enet glm imgui)

    # Loop through each target and set its folder property
    foreach(target ${EXTERNAL_TARGETS})
        set_property(TARGET ${target} PROPERTY FOLDER "External")
    endforeach()

    # Move glfw-related projects under External/GLFW3

    # Collect targets from the glfw root directory
    get_property(ROOT_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)

    # Collect targets from the glfw 'src' directory
    get_property(SRC_SUBPROJECTS DIRECTORY "${glfw_SOURCE_DIR}/src" PROPERTY BUILDSYSTEM_TARGETS)

    # Combine both sets of subprojects into one list
    set(GLFW_SUBPROJECTS ${ROOT_SUBPROJECTS} ${SRC_SUBPROJECTS})

    # Loop through the combined list of subprojects
    foreach(subproject ${GLFW_SUBPROJECTS})
        set_property(TARGET ${subproject} PROPERTY FOLDER "External/GLFW3")
    endforeach()

    # Function to create source groups by full directory hierarchy and remove unnecessary prefix
    function(create_source_groups_by_dir root_dir base_group remove_prefix)
        file(GLOB_RECURSE files LIST_DIRECTORIES false "${root_dir}/*")

        foreach(file_path ${files})
            file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}" "${file_path}")
            get_filename_component(dir "${relative_path}" DIRECTORY)
            string(REPLACE "/" "\\" group_path "${dir}")
        
            # Remove the unnecessary prefix
            string(REPLACE "${remove_prefix}" "" group_path "${group_path}")

            # Add base group if provided
            if (NOT "${base_group}" STREQUAL "")
                set(group_path "${base_group}\\${group_path}")
            endif()

            source_group("${group_path}" FILES "${file_path}")
        endforeach()
    endfunction()

    # Call the function for source and header files
    # Removing "Engine/Source" and "Engine/Include" from the beginning of the path
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source" "Source" "Engine\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include" "Include" "Engine\\Include")

    # Manually include glm headers in a visible group
    file(GLOB_RECURSE GLM_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/External/glm/glm/*.hpp")
    source_group("External\\glm" FILES ${GLM_HEADERS})
endif()

# Suppress Winsock deprecation warnings globally for the Engine target
target_compile_definitions(Engine PUBLIC _WINSOCK_DEPRECATED_NO_WARNINGS)