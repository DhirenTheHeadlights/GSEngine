cmake_minimum_required(VERSION 4.2)

project(GSEditor)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Editor")
set(EDITOR_SRC "${EDITOR_DIR}/Source")
set(MAIN_CPP "${EDITOR_SRC}/Main.cpp")

set(EDITOR_SEMANTIC_CONTRACT "${CMAKE_CURRENT_SOURCE_DIR}/Tests/SemanticContract.cpp")
set(EDITOR_SEMANTIC_AUDIT "${EDITOR_SRC}/Tools/SemanticAudit.cpp")

# gse.ide.config used to be generated here by configure_file; it is a real source
# now. Existing build trees still hold the generated copy, which the binary-dir
# glob below would collect alongside the source one - a duplicate module
# definition. This must stay above the glob to have any effect.
file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/Editor/Import/Config.cppm")

file(GLOB_RECURSE EDITOR_MODULES CONFIGURE_DEPENDS "${EDITOR_DIR}/*.cppm" "${CMAKE_CURRENT_BINARY_DIR}/Editor/*.cppm")
list(REMOVE_ITEM EDITOR_MODULES "${EDITOR_DIR}/Import/Ide.cppm")

# Module implementation units (.cpp) live in EditorLib. Main.cpp is the executable
# and gse_tokens_plugin.cpp is compiled separately into a GCC plugin below, so both
# are excluded here.
file(GLOB_RECURSE EDITOR_IMPLS CONFIGURE_DEPENDS "${EDITOR_DIR}/*.cpp")
list(REMOVE_ITEM EDITOR_IMPLS "${MAIN_CPP}")
list(REMOVE_ITEM EDITOR_IMPLS "${EDITOR_SRC}/Analysis/Plugin/gse_tokens_plugin.cpp")
list(REMOVE_ITEM EDITOR_IMPLS "${EDITOR_SEMANTIC_CONTRACT}")
list(REMOVE_ITEM EDITOR_IMPLS "${EDITOR_SEMANTIC_AUDIT}")

# A .cpp that declares an implementation partition (module gse.x:y;) provides a
# module and must live in a CXX_MODULES file set; a plain implementation unit
# (module gse.x;) or global-module .cpp stays an ordinary source.
set(EDITOR_MODULE_IMPLS "")
set(EDITOR_PLAIN_IMPLS "")
foreach(_impl IN LISTS EDITOR_IMPLS)
    file(STRINGS "${_impl}" _part_decl REGEX "^module gse\\.[A-Za-z_0-9.]+:[A-Za-z_0-9]+ *;" LIMIT_COUNT 1)
    if(_part_decl)
        list(APPEND EDITOR_MODULE_IMPLS "${_impl}")
    else()
        list(APPEND EDITOR_PLAIN_IMPLS "${_impl}")
    endif()
endforeach()

add_library(EditorLib)

target_sources(EditorLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${EDITOR_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/Editor"
        FILES ${EDITOR_MODULES}
)

target_sources(EditorLib
    PUBLIC
        FILE_SET cxx_modules_public TYPE CXX_MODULES
        BASE_DIRS "${EDITOR_DIR}/Import"
        FILES "${EDITOR_DIR}/Import/Ide.cppm"
)

if(EDITOR_MODULE_IMPLS)
    target_sources(EditorLib
        PRIVATE
            FILE_SET editor_module_impls TYPE CXX_MODULES
            BASE_DIRS "${EDITOR_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/Editor"
            FILES ${EDITOR_MODULE_IMPLS}
    )
endif()

if(EDITOR_PLAIN_IMPLS)
    target_sources(EditorLib PRIVATE ${EDITOR_PLAIN_IMPLS})
endif()

target_compile_features(EditorLib PUBLIC cxx_std_26)

target_include_directories(EditorLib PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Editor"
)

target_link_libraries(EditorLib PRIVATE Engine)

add_executable(Editor "${MAIN_CPP}")
target_link_libraries(Editor PRIVATE EditorLib Engine)

add_executable(SemanticAudit "${EDITOR_SEMANTIC_AUDIT}")
target_link_libraries(SemanticAudit PRIVATE EditorLib Engine)

# MinGW links the console subsystem by default, so Windows allocates a console host
# (Windows Terminal on 11) alongside the editor's own window. The GUI subsystem drops it.
# main() still links: mingw-w64's crt2.o exports WinMainCRTStartup next to mainCRTStartup
# and both route to __tmainCRTStartup, which calls main.
if(WIN32)
    set_target_properties(Editor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# GCC-trunk's C++ modules codegen on MinGW (PE/COFF) emits vague-linkage function-local statics
# as STRONG symbols in every importing TU instead of COMDAT-folding them, so the final link sees
# duplicate definitions. The offenders are overwhelmingly in external code we can't touch:
# libstdc++ (`std::_Sp_make_shared_tag::_S_ti()::__tag`) and vulkan-hpp (`vk::errorCategory()::
# instance`, pulled in by its throwing API which the engine relies on). --allow-multiple-definition
# makes ld keep the first (identical) copy, i.e. it performs the COMDAT fold GCC failed to emit.
# Safe here; remove only once the GCC modules vague-linkage bug is fixed.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Editor PRIVATE "-Wl,--allow-multiple-definition")
    target_link_options(SemanticAudit PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Editor SemanticAudit PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

# Build the gse_tokens GCC plugin (gse.ide.analysis's Plugin/) into a DLL next to the editor.
# The editor spawns g++ with -fplugin=<this> to extract semantic tokens from the compiler's own
# AST. It is version-locked to the compiler, so it is rebuilt from source here rather than shipped.
# Needs a toolchain built with --enable-plugin (which generates cc1plus.exe.a); skipped otherwise.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(GSE_TOKENS_PLUGIN_SRC "${EDITOR_SRC}/Analysis/Plugin/gse_tokens_plugin.cpp")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=plugin
        OUTPUT_VARIABLE _gse_plugin_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS "${_gse_plugin_dir}/cc1plus.exe.a")
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gse_tokens.dll"
            COMMAND "${CMAKE_CXX_COMPILER}"
                "-I${_gse_plugin_dir}/include"
                -O2 -fno-rtti -shared
                "${GSE_TOKENS_PLUGIN_SRC}"
                "${_gse_plugin_dir}/cc1plus.exe.a"
                -o "${CMAKE_CURRENT_BINARY_DIR}/gse_tokens.dll"
            # A GCC plugin is ABI-locked to the compiler that built it: cc1plus refuses to load
            # one from a different build and exits 1 before compiling anything, which surfaces as
            # every analyzed TU failing identically rather than as a stale-artifact error. The
            # toolchain must therefore be a declared input, or a compiler swap leaves the plugin
            # silently mismatched because its source did not change.
            DEPENDS
                "${GSE_TOKENS_PLUGIN_SRC}"
                "${CMAKE_CXX_COMPILER}"
                "${_gse_plugin_dir}/cc1plus.exe.a"
            COMMENT "Building gse_tokens GCC plugin"
            VERBATIM
        )
        set(_gse_contract_output "${CMAKE_CURRENT_BINARY_DIR}/gse_tokens_contract.tsv")
        set(_gse_contract_stamp "${CMAKE_CURRENT_BINARY_DIR}/gse_tokens_contract.stamp")
        add_custom_command(
            OUTPUT "${_gse_contract_stamp}"
            BYPRODUCTS "${_gse_contract_output}"
            COMMAND "${CMAKE_CXX_COMPILER}"
                -std=c++26 -fsyntax-only
                "-fplugin=${CMAKE_CURRENT_BINARY_DIR}/gse_tokens.dll"
                "-fplugin-arg-gse_tokens-out=${_gse_contract_output}"
                "-fplugin-arg-gse_tokens-root=${CMAKE_SOURCE_DIR}"
                "${EDITOR_SEMANTIC_CONTRACT}"
            COMMAND "${CMAKE_COMMAND}"
                "-DCONTRACT_OUTPUT=${_gse_contract_output}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/CheckSemanticContract.cmake"
            COMMAND "${CMAKE_COMMAND}" -E touch "${_gse_contract_stamp}"
            DEPENDS
                "${CMAKE_CURRENT_BINARY_DIR}/gse_tokens.dll"
                "${EDITOR_SEMANTIC_CONTRACT}"
                "${CMAKE_CURRENT_SOURCE_DIR}/Tests/CheckSemanticContract.cmake"
            VERBATIM
        )
        add_custom_target(gse_tokens_plugin ALL DEPENDS "${_gse_contract_stamp}")
        add_custom_target(gse_tokens_contract DEPENDS "${_gse_contract_stamp}")
        add_dependencies(Editor gse_tokens_plugin)
        add_dependencies(SemanticAudit gse_tokens_plugin)
    else()
        message(WARNING "gse_tokens plugin: ${_gse_plugin_dir}/cc1plus.exe.a not found, so the plugin will not be built. The editor still compiles and runs, but syntax highlighting, hover, go-to-definition and the symbol index are all plugin-authoritative and will be silently empty. Install a GCC configured with --enable-plugin and reconfigure.")
    endif()
endif()

gse_copy_runtime_deps(Editor)
