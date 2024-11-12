cmake_minimum_required(VERSION 3.16)

# Set project details and C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/Engine/External" CACHE STRING "Base directory for FetchContent dependencies" FORCE)

# Ensure the directory exists
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")

# Include FetchContent
include(FetchContent)

# Gather engine source files (cpp) from the new "Resource" folder
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/*.cpp")

# Gather engine header files (h, hpp) from the new "Include" folder
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/*.h")

# Add the engine as a static library including source and header files
add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Add the third-party libraries as subdirectories

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

##############################################

##############################################
# FetchContent_Declare for glad
##############################################

option(GLAD_GENERATE "Generate GLAD source files" OFF)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)

FetchContent_MakeAvailable(glad)

############################################

############################################
# FetchContent_Declare for stb_image and stb_truetype
############################################

# stb_image
FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master 
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_MakeAvailable(stb_image)

# stb_truetype (from the same stb repository)
FetchContent_Declare(
    stb_truetype
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_MakeAvailable(stb_truetype)

############################################

############################################
# FetchContent_Declare for enet
############################################

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.17
)

FetchContent_MakeAvailable(enet)

target_include_directories(Engine PUBLIC ${enet_SOURCE_DIR}/include)

############################################

############################################
# FetchContent_Declare for miniaudio
############################################

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(miniaudio)

# Include miniaudio in the Engine target
target_include_directories(Engine PUBLIC ${miniaudio_SOURCE_DIR})

############################################

############################################
# FetchContent_Declare for GLM
############################################

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(glm)

############################################

add_subdirectory(External/imgui-docking)

# Include directories for engine headers
target_include_directories(
    Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/"
    ${stb_image_SOURCE_DIR}
    ${stb_truetype_SOURCE_DIR}
    ${glad_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
)

# Include directories for third-party libraries
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
)

# Visual Studio specific settings
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP) # Multi-core compilation

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
    # Removing "Engine/Include" from the beginning of the path
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source" "Source" "Engine\\Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include" "Include" "Engine\\Include")

    # Manually include glm headers in a visible group
    file(GLOB_RECURSE GLM_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/External/glm/glm/*.hpp")
    source_group("External\\glm" FILES ${GLM_HEADERS})
endif()

# Define macros for resource paths based on the build type
if(PRODUCTION_BUILD)
    target_compile_definitions(Engine PUBLIC RESOURCES_PATH="./Resources/" PRODUCTION_BUILD = 1)
    target_compile_definitions(Engine PUBLIC IMGUI = 0)
else()
    target_compile_definitions(Engine PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Resources/" PRODUCTION_BUILD = 0)
    target_compile_definitions(Engine PUBLIC IMGUI = 1)
endif()
