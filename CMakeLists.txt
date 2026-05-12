cmake_minimum_required(VERSION 4.2)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "" FORCE)

if(CMAKE_HOST_WIN32)
    if(NOT DEFINED ENV{CLANG_P2996_ROOT} OR "$ENV{CLANG_P2996_ROOT}" STREQUAL "")
        message(FATAL_ERROR "CLANG_P2996_ROOT is not set - run scripts/install_clang_p2996.py")
    endif()

    if(NOT DEFINED ENV{VCToolsInstallDir} OR NOT DEFINED ENV{WindowsSdkDir} OR NOT DEFINED ENV{WindowsSDKVersion} OR NOT DEFINED ENV{UniversalCRTSdkDir} OR NOT DEFINED ENV{UCRTVersion})
        message(FATAL_ERROR "MSVC env vars not set - launch from a Developer Command Prompt or Developer PowerShell")
    endif()

    file(TO_CMAKE_PATH "$ENV{CLANG_P2996_ROOT}" _gse_clang_root)

    set(_gse_libcxx_inc "${_gse_clang_root}/include/c++/v1")
    set(_gse_libcxx_lib "${_gse_clang_root}/lib/c++.lib")
    if(NOT EXISTS "${_gse_libcxx_inc}/vector")
        message(FATAL_ERROR "libc++ headers not found at ${_gse_libcxx_inc} - run scripts/build_libcxx_p2996.py")
    endif()

    set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
        "${_gse_clang_root}/bin/clang-scan-deps.exe"
        CACHE FILEPATH "")

    string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdinc++ -Xclang -cxx-isystem -Xclang \"${_gse_libcxx_inc}\"")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -D__GCC_DESTRUCTIVE_SIZE=64 -D__GCC_CONSTRUCTIVE_SIZE=64")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -Xclang --dependent-lib=ucrt")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -D_STATIC_INLINE_UCRT_FUNCTIONS=0")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -freflection-latest")
    string(APPEND CMAKE_C_FLAGS_INIT   " -Xclang --dependent-lib=ucrt")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT    " \"${_gse_libcxx_lib}\"")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " \"${_gse_libcxx_lib}\"")
endif()

project(GSEngine)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC trunk treats -Wchanges-meaning as an error in some cases (member
    # shadowing a type from an imported module). The codebase intentionally
    # uses members named after their type (e.g., `separation separation;`).
    add_compile_options(-Wno-changes-meaning -Wno-error=changes-meaning)
endif()

if(CMAKE_HOST_WIN32)
    set(_gse_libcxx_modules_dir "${_gse_clang_root}/share/libc++/v1")
    if(NOT EXISTS "${_gse_libcxx_modules_dir}/std.cppm")
        message(FATAL_ERROR "libc++ std.cppm not found at ${_gse_libcxx_modules_dir} - rebuild libc++ with LIBCXX_INSTALL_MODULES=ON")
    endif()

    add_library(__cmake_libcxx_std STATIC)
    set_property(TARGET __cmake_libcxx_std PROPERTY EXCLUDE_FROM_ALL 1)
    set_property(TARGET __cmake_libcxx_std PROPERTY CXX_SCAN_FOR_MODULES 1)
    set_property(TARGET __cmake_libcxx_std PROPERTY CXX_MODULE_STD 0)
    target_compile_features(__cmake_libcxx_std PUBLIC cxx_std_26)
    target_include_directories(__cmake_libcxx_std SYSTEM PRIVATE "${_gse_libcxx_inc}")
    target_sources(__cmake_libcxx_std
        PUBLIC
            FILE_SET std TYPE CXX_MODULES
            BASE_DIRS "${_gse_libcxx_modules_dir}"
            FILES
                "${_gse_libcxx_modules_dir}/std.cppm"
                "${_gse_libcxx_modules_dir}/std.compat.cppm"
    )
    if(NOT TARGET __CMAKE::CXX23)
        add_library(__CMAKE::CXX23 ALIAS __cmake_libcxx_std)
    endif()
    if(NOT TARGET __CMAKE::CXX26)
        add_library(__CMAKE::CXX26 ALIAS __cmake_libcxx_std)
    endif()
endif()

function(gse_copy_libcxx_runtime target)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    file(GLOB _libcxx_runtime_dlls
        "${_gse_clang_root}/bin/c++.dll"
        "${_gse_clang_root}/bin/c++abi.dll"
        "${_gse_clang_root}/bin/libc++.dll"
        "${_gse_clang_root}/bin/libc++abi.dll"
        "${_gse_clang_root}/bin/unwind.dll"
        "${_gse_clang_root}/bin/libunwind.dll"
    )
    foreach(_dll IN LISTS _libcxx_runtime_dlls)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
    endforeach()
endfunction()

function(gse_copy_runtime_deps target)
    gse_copy_libcxx_runtime(${target})
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    if(VCPKG_BUILD_TYPE STREQUAL "release" OR VCPKG_TARGET_TRIPLET MATCHES "-release$")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endif()
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize=address" OR CMAKE_CXX_FLAGS_INIT MATCHES "fsanitize=address")
        file(GLOB _gse_asan_dll "$ENV{CLANG_P2996_ROOT}/lib/clang/*/lib/x86_64-pc-windows-msvc/clang_rt.asan_dynamic.dll")
        if(_gse_asan_dll)
            list(GET _gse_asan_dll 0 _gse_asan_dll)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_gse_asan_dll}" "$<TARGET_FILE_DIR:${target}>"
            )
        else()
            message(WARNING "ASAN build requested but clang_rt.asan_dynamic.dll not found under $ENV{CLANG_P2996_ROOT}/lib/clang/*/lib/x86_64-pc-windows-msvc/. Run bootstrap.py to build compiler-rt.")
        endif()
    endif()
endfunction()

add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(Server)
