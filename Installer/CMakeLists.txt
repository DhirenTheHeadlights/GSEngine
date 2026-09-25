cmake_minimum_required(VERSION 4.2)

project(GSEngineInstaller)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INSTALLER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE INSTALLER_MODULES CONFIGURE_DEPENDS "${INSTALLER_SRC}/*.cppm")

add_executable(Installer "${INSTALLER_SRC}/Main.cpp")

target_sources(Installer
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${INSTALLER_SRC}"
        FILES ${INSTALLER_MODULES}
)

target_link_libraries(Installer PRIVATE Engine)

# GUI subsystem: the setup exe must not open a console next to its window.
# main() still links; mingw-w64's crt routes WinMainCRTStartup to it.
if(WIN32)
    set_target_properties(Installer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Same vague-linkage fold the Editor and Sandbox executables need; see Editor/CMakeLists.txt.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Installer PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Installer PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Installer)

add_executable(Setup "${CMAKE_CURRENT_SOURCE_DIR}/Setup/Main.cpp")
target_link_libraries(Setup PRIVATE Engine)
target_link_options(Setup PRIVATE -static -Wl,--gc-sections)
if(WIN32)
    set_target_properties(Setup PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Setup PRIVATE "-Wl,--allow-multiple-definition")
endif()
