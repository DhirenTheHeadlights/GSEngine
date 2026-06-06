cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "" FORCE)

# Put the gcc-trunk toolchain's bin on PATH for the whole configure, including
# vcpkg's dependency builds: the compiler driver spawns cc1/cc1plus, which load
# the toolchain's runtime DLLs from bin, so without it compilation fails. Runs on
# every configure, so it survives build-triggered reconfigures where the preset
# environment isn't applied. No-op if the toolchain isn't installed there.
if(CMAKE_HOST_WIN32 AND EXISTS "$ENV{USERPROFILE}/.gcc-trunk/current/bin/x86_64-w64-mingw32-gcc.exe")
    file(TO_NATIVE_PATH "$ENV{USERPROFILE}/.gcc-trunk/current/bin" _gcc_trunk_bin)
    set(ENV{PATH} "${_gcc_trunk_bin};$ENV{PATH}")
endif()

project(GSEngine)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-freflection -Wno-changes-meaning -Wno-error=changes-meaning)
endif()

function(gse_copy_runtime_deps target)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    if(EXISTS "${_vcpkg_bin}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_vcpkg_bin}"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endif()
endfunction()

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Server)
add_subdirectory(Editor)
