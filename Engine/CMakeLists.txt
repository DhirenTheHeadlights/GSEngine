cmake_minimum_required(VERSION 3.16)

# Set project details and C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Engine)

# Add the third-party libraries as subdirectories

# Disable building GLFW examples, docs, and tests
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW: Build documentation")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW: Build examples")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW: Build tests")
set(GLFW_INSTALL OFF CACHE BOOL "Do not generate installation target")

# Add GLFW as a subdirectory
add_subdirectory(External/glfw-3.3.2)

add_subdirectory(External/glad)
add_subdirectory(External/stb_image)
add_subdirectory(External/stb_truetype)
add_subdirectory(External/enet-1.3.17)
add_subdirectory(External/raudio)
add_subdirectory(External/glm)
add_subdirectory(External/imgui-docking)

# Gather engine source files (cpp) from the new "Resource" folder
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/*.cpp")

# Gather engine header files (h, hpp) from the new "Include" folder
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include/*.hpp")

# Add the engine as a static library including source and header files
add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Include directories for engine headers
target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Link the third-party libraries to the engine
target_link_libraries(Engine PUBLIC
    glfw
    glad
    raudio
    imgui
    enet
    stb_image
    stb_truetype
    glm
)

# Visual Studio specific settings
if (MSVC)
    # Group source files under "Source Files"
    source_group("Source Files" FILES ${ENGINE_SOURCES})

    # Group header files under "Header Files"
    source_group("Header Files" FILES ${ENGINE_HEADERS})

    # Group third-party libraries under "ThirdParty"
    set_property(TARGET glfw PROPERTY FOLDER "External")
    set_property(TARGET glad PROPERTY FOLDER "External")
    set_property(TARGET enet PROPERTY FOLDER "External")
    set_property(TARGET imgui PROPERTY FOLDER "External")
    set_property(TARGET raudio PROPERTY FOLDER "External")
    set_property(TARGET stb_image PROPERTY FOLDER "External")
    set_property(TARGET stb_truetype PROPERTY FOLDER "External")
    set_property(TARGET glm PROPERTY FOLDER "External")

    # Function to create source groups by full directory hierarchy
    function(create_source_groups_by_dir root_dir base_group)
        file(GLOB_RECURSE files LIST_DIRECTORIES false "${root_dir}/*")

        foreach(file_path ${files})
            file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}" "${file_path}")
            get_filename_component(dir "${relative_path}" DIRECTORY)
            string(REPLACE "/" "\\" group_path "${dir}")
        
            # Add base group if provided
            if (NOT "${base_group}" STREQUAL "")
                set(group_path "${base_group}\\${group_path}")
            endif()

            source_group("${group_path}" FILES "${file_path}")
        endforeach()
    endfunction()

    # Call the function for source and header files
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source" "Source")
    create_source_groups_by_dir("${CMAKE_CURRENT_SOURCE_DIR}/Engine/Include" "Include")
endif()

# Exclude dependencies from all builds
set_target_properties(glfw PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(glad PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(enet PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(glm PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Define macros for resource paths based on the build type
if(PRODUCTION_BUILD)
    target_compile_definitions(Engine PUBLIC RESOURCES_PATH="./Resources/" PRODUCTION_BUILD=1)
else()
    target_compile_definitions(Engine PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Resources/" PRODUCTION_BUILD=0)
endif()
