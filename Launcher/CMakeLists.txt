cmake_minimum_required(VERSION 4.2)

project(GSEngineLauncher)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LAUNCHER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE LAUNCHER_MODULES CONFIGURE_DEPENDS "${LAUNCHER_SRC}/*.cppm")

add_executable(Launcher "${LAUNCHER_SRC}/Main.cpp")

target_sources(Launcher
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${LAUNCHER_SRC}"
        FILES ${LAUNCHER_MODULES}
)

target_link_libraries(Launcher PRIVATE Engine)

# GUI subsystem: the launcher must not open a console next to its window.
if(WIN32)
    set_target_properties(Launcher PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Same vague-linkage fold the Editor and Sandbox executables need; see Editor/CMakeLists.txt.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Launcher PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Launcher PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Launcher)
