# Everything a project needs to consume the engine from source.
#
# Include this BEFORE project() -- it sets cache variables and environment that
# compiler detection depends on:
#
#   include("<engine>/Engine/cmake/GSEEngine.cmake")
#   project(MyGame)
#   gse_configure_compiler()
#   add_subdirectory("${GSE_ENGINE_ROOT}/Engine" "${CMAKE_BINARY_DIR}/Engine")
#   gse_write_manifest()
#   ...
#   gse_copy_runtime_deps(MyGame)
#
# The engine repo itself includes this too, so there is one definition of each
# of these rather than a copy per consumer.

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "" FORCE)
set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

get_filename_component(_gse_engine_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GSE_ENGINE_ROOT "${_gse_engine_root}" CACHE INTERNAL "Root of the GSE engine tree")

# Put the gcc-trunk toolchain's bin on PATH for the whole configure, including
# vcpkg's dependency builds: the compiler driver spawns cc1/cc1plus, which load
# the toolchain's runtime DLLs from bin, so without it compilation fails. Runs on
# every configure, so it survives build-triggered reconfigures where the preset
# environment isn't applied. No-op if the toolchain isn't installed there.
if(CMAKE_HOST_WIN32 AND EXISTS "$ENV{USERPROFILE}/.gcc-trunk/current/bin/x86_64-w64-mingw32-gcc.exe")
    file(TO_NATIVE_PATH "$ENV{USERPROFILE}/.gcc-trunk/current/bin" _gcc_trunk_bin)
    set(ENV{PATH} "${_gcc_trunk_bin};$ENV{PATH}")
endif()

function(gse_configure_compiler)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-freflection -fcontracts -mavx2 -Wno-changes-meaning -Wno-error=changes-meaning)
        add_compile_options($<$<CONFIG:Release>:-fcontract-evaluation-semantic=ignore>)
        add_compile_definitions($<$<CONFIG:Release>:GSE_CONTRACTS_IGNORED=1>)
        add_link_options(-fcontracts)
    endif()
endfunction()

# Marker consumed by gse.config at runtime: the first directory containing it,
# walking up from the executable, is the build root. `root` must name the ENGINE
# tree, not the consuming project -- it is where engine resources and shaders are
# read from. `project` is the other half: the tree a running game writes its own
# data into, defaulting to this build's source dir. A build tree that holds the
# engine plus a game beside it must name the game, since the source dir is then
# the engine. An install image ships its own with mode = installed. Nothing is
# baked into the binaries themselves.
function(gse_write_manifest)
    set(_project "${CMAKE_SOURCE_DIR}")
    if(ARGC GREATER 0)
        set(_project "${ARGV0}")
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}/gse.manifest" "mode = dev\nroot = ${GSE_ENGINE_ROOT}\nproject = ${_project}\n")
endfunction()

function(gse_copy_runtime_deps target)
    if(NOT CMAKE_HOST_WIN32)
        return()
    endif()
    set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    if(EXISTS "${_vcpkg_bin}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${_vcpkg_bin}"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endif()

    # The GCC toolchain's own runtime DLLs live in the compiler's bin dir, not vcpkg's.
    # RPATH is an ELF concept the Windows PE loader ignores, so the BUILD_RPATH set on the
    # exe can't locate them -- without copying them next to the exe, launching it outside
    # the preset's PATH exits 127 (STATUS_DLL_NOT_FOUND; here libmcfgthread-2.dll, the only
    # one not linked statically). Copy the C++/threading runtime DLLs that exist; harmless
    # when one is actually linked statically.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(_runtime_dll IN ITEMS
                libmcfgthread-2.dll
                libstdc++-6.dll
                libgcc_s_seh-1.dll
                libwinpthread-1.dll)
            if(EXISTS "${_gcc_bin_dir}/${_runtime_dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_gcc_bin_dir}/${_runtime_dll}"
                        "$<TARGET_FILE_DIR:${target}>"
                )
            endif()
        endforeach()
    endif()

    # DirectX 12 Agility SDK runtime, fetched in Engine/CMakeLists.txt. The exe's exported
    # D3D12SDKPath points at this D3D12/ subfolder, so the OS loader uses this runtime instead
    # of the machine's.
    if(GSE_AGILITY_BIN_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GSE_AGILITY_BIN_DIR}/D3D12Core.dll"
                "$<TARGET_FILE_DIR:${target}>/D3D12/"
        )
        if(EXISTS "${GSE_AGILITY_BIN_DIR}/d3d12SDKLayers.dll")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GSE_AGILITY_BIN_DIR}/d3d12SDKLayers.dll"
                    "$<TARGET_FILE_DIR:${target}>/D3D12/"
            )
        endif()
    endif()
endfunction()

# The gse_tokens GCC plugin extracts semantic tokens from the compiler's own AST; the
# editor and any project that highlights C++ spawn g++ with -fplugin=<it>. A plugin is
# ABI-locked to the compiler that built it: cc1plus refuses to load one from a different
# build and exits 1 before compiling anything, which surfaces as every compile failing
# identically rather than as a stale-artifact error. So it is rebuilt from source per
# build tree with the toolchain as a declared input, and every consumer gets a
# toolchain.json beside its exe naming the compiler and plugin that belong together.
# Needs a GCC configured with --enable-plugin (which generates cc1plus.exe.a).
function(_gse_add_token_plugin)
    set(_source_dir "${GSE_ENGINE_ROOT}/Engine/Tools/TokenPlugin")
    set(_out_dir "${CMAKE_BINARY_DIR}/TokenPlugin")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=plugin
        OUTPUT_VARIABLE _plugin_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT EXISTS "${_plugin_dir}/cc1plus.exe.a")
        message(WARNING "gse_tokens plugin: ${_plugin_dir}/cc1plus.exe.a not found, so the plugin will not be built. Projects still compile and run, but semantic highlighting, hover, go-to-definition and the symbol index are all plugin-authoritative and will be silently empty. Install a GCC configured with --enable-plugin and reconfigure.")
        return()
    endif()
    add_custom_command(
        OUTPUT "${_out_dir}/gse_tokens.dll"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_out_dir}"
        COMMAND "${CMAKE_CXX_COMPILER}"
            "-I${_plugin_dir}/include"
            -O2 -fno-rtti -shared
            "${_source_dir}/gse_tokens_plugin.cpp"
            "${_plugin_dir}/cc1plus.exe.a"
            -o "${_out_dir}/gse_tokens.dll"
        DEPENDS
            "${_source_dir}/gse_tokens_plugin.cpp"
            "${CMAKE_CXX_COMPILER}"
            "${_plugin_dir}/cc1plus.exe.a"
        COMMENT "Building gse_tokens GCC plugin"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${_out_dir}/gse_tokens_contract.stamp"
        BYPRODUCTS "${_out_dir}/gse_tokens_contract.tsv"
        COMMAND "${CMAKE_CXX_COMPILER}"
            -std=c++26 -fsyntax-only
            "-fplugin=${_out_dir}/gse_tokens.dll"
            "-fplugin-arg-gse_tokens-out=${_out_dir}/gse_tokens_contract.tsv"
            "-fplugin-arg-gse_tokens-root=${GSE_ENGINE_ROOT}"
            "${_source_dir}/SemanticContract.cpp"
        COMMAND "${CMAKE_COMMAND}"
            "-DCONTRACT_OUTPUT=${_out_dir}/gse_tokens_contract.tsv"
            -P "${_source_dir}/CheckSemanticContract.cmake"
        COMMAND "${CMAKE_COMMAND}" -E touch "${_out_dir}/gse_tokens_contract.stamp"
        DEPENDS
            "${_out_dir}/gse_tokens.dll"
            "${_source_dir}/SemanticContract.cpp"
            "${_source_dir}/CheckSemanticContract.cmake"
        VERBATIM
    )
    add_custom_target(gse_tokens_plugin ALL DEPENDS "${_out_dir}/gse_tokens_contract.stamp")
    add_custom_target(gse_tokens_contract DEPENDS "${_out_dir}/gse_tokens_contract.stamp")
endfunction()

function(gse_token_plugin target)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        return()
    endif()
    if(NOT TARGET gse_tokens_plugin)
        _gse_add_token_plugin()
    endif()
    if(NOT TARGET gse_tokens_plugin)
        return()
    endif()
    add_custom_target(gse_tokens_plugin_${target}
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/TokenPlugin/gse_tokens.dll"
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
    add_dependencies(gse_tokens_plugin_${target} gse_tokens_plugin)
    add_dependencies(${target} gse_tokens_plugin_${target})
    file(GENERATE
        OUTPUT "$<TARGET_FILE_DIR:${target}>/toolchain.json"
        CONTENT "{\n\t\"compiler\": \"${CMAKE_CXX_COMPILER}\",\n\t\"plugin\": \"$<TARGET_FILE_DIR:${target}>/gse_tokens.dll\"\n}\n"
    )
endfunction()
