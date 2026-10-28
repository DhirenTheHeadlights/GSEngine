cmake_minimum_required(VERSION 4.2)

project(SandboxProject)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SANDBOX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Sandbox")
set(SANDBOX_SRC "${SANDBOX_DIR}/Source")
set(MAIN_CPP "${SANDBOX_SRC}/Main.cpp")

file(GLOB_RECURSE SANDBOX_MODULES CONFIGURE_DEPENDS "${SANDBOX_DIR}/*.cppm")

file(GLOB_RECURSE SANDBOX_IMPLS CONFIGURE_DEPENDS "${SANDBOX_DIR}/*.cpp")
list(REMOVE_ITEM SANDBOX_IMPLS "${MAIN_CPP}")

gse_split_module_impls(SANDBOX_IMPLS SANDBOX_MODULE_IMPLS SANDBOX_PLAIN_IMPLS)

add_library(SandboxLib)

target_sources(SandboxLib
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${SANDBOX_DIR}"
        FILES ${SANDBOX_MODULES}
)

if(SANDBOX_MODULE_IMPLS)
    target_sources(SandboxLib
        PRIVATE
            FILE_SET cxx_module_impls TYPE CXX_MODULES
            BASE_DIRS "${SANDBOX_DIR}"
            FILES ${SANDBOX_MODULE_IMPLS}
    )
endif()

if(SANDBOX_PLAIN_IMPLS)
    target_sources(SandboxLib PRIVATE ${SANDBOX_PLAIN_IMPLS})
endif()

target_include_directories(SandboxLib PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Sandbox"
)

target_link_libraries(SandboxLib PRIVATE Engine EngineServer)

add_executable(Sandbox "${MAIN_CPP}")
target_link_libraries(Sandbox PRIVATE SandboxLib Engine EngineServer)

# A wasm32 module has no argv: the harness calls its exported `entry`, which hands main the
# scenario this configuration was set up for plus whatever GSE_RUN_ARGS carries at run time.
#
# It calls main rather than assembling the bench configuration itself, because
# apply_scenario turning a name into a scene, frame counts and pinned settings IS the world
# the parity hash compares, and a second copy of it would drift. GCC's wasm backend names a
# main(int, char**) definition __main_argc_argv, the wasi-libc entry name, which is the
# symbol called. --no-engine-gpu is the engine's own headless flag: a wasm32 build compiles
# no GPU backend, so without it the run dereferences the null device it was handed.
# GSE_RUN_ARGS is split on spaces and read at run time, so a diagnostic flag costs no
# relink.
if(NOT WIN32)
    set(GSE_WASM_SCENARIO "scene_reset_cpu" CACHE STRING "Scenario the wasm32 module's entry runs")
    configure_file("${SANDBOX_SRC}/Web/Entry.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/WebEntry.cpp" @ONLY)
    target_sources(Sandbox PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/WebEntry.cpp")

    # What runs the module, written beside it: `node Sandbox.mjs ARGS` under node, and
    # `node serve.mjs --open` in a browser. The run reports the native build's executable
    # to gse.config so the log name, user state and baked resources are the native run's.
    set(GSE_WASM_NATIVE_BUILD "x64-mingw-gcc-Release" CACHE STRING "Native build tree whose manifest and baked resources the wasm32 run resolves")
    set(_gse_wasm_host "${CMAKE_SOURCE_DIR}/Engine/cmake/wasm")
    get_filename_component(_gse_native_build "${CMAKE_BINARY_DIR}/../${GSE_WASM_NATIVE_BUILD}" ABSOLUTE)
    file(RELATIVE_PATH _gse_native_exe "${CMAKE_SOURCE_DIR}" "${_gse_native_build}/Sandbox.exe")
    set(_gse_run_dir ".generated/.gcc-wasm-run")
    file(RELATIVE_PATH _gse_project "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    set(GSE_WASM_RUN_MODULE "Sandbox.wasm")
    set(GSE_WASM_RUN_ROOT "${CMAKE_SOURCE_DIR}")
    set(GSE_WASM_RUN_EXECUTABLE "/${_gse_native_exe}")
    set(GSE_WASM_RUN_PROJECT "/${_gse_project}")
    set(GSE_WASM_RUN_USER "/${_gse_run_dir}/user")
    set(GSE_WASM_RUN_STATE "/${_gse_run_dir}/state")
    set(GSE_WASM_RUN_PROJECTS "/${_gse_run_dir}/projects")
    configure_file("${_gse_wasm_host}/wasm_run.json.in" "${CMAKE_CURRENT_BINARY_DIR}/wasm_run.json" @ONLY)
    configure_file("${_gse_wasm_host}/launcher.mjs" "${CMAKE_CURRENT_BINARY_DIR}/Sandbox.mjs" COPYONLY)
    foreach(_gse_file node_host.mjs serve.mjs page/index.html page/page.mjs page/host.mjs page/guest.mjs page/protocol.mjs)
        configure_file("${_gse_wasm_host}/${_gse_file}" "${CMAKE_CURRENT_BINARY_DIR}/${_gse_file}" COPYONLY)
    endforeach()
endif()

# GCC-trunk's C++ modules codegen on MinGW (PE/COFF) emits vague-linkage function-local statics
# as STRONG symbols in every importing TU instead of COMDAT-folding them, so the final link sees
# duplicate definitions. The offenders are overwhelmingly in external code we can't touch:
# libstdc++ (`std::_Sp_make_shared_tag::_S_ti()::__tag`) and vulkan-hpp (`vk::errorCategory()::
# instance`, pulled in by its throwing API which the engine relies on). --allow-multiple-definition
# makes ld keep the first (identical) copy, i.e. it performs the COMDAT fold GCC failed to emit.
# Safe here; remove only once the GCC modules vague-linkage bug is fixed.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(Sandbox PRIVATE "-Wl,--allow-multiple-definition")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_gcc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_gcc_root "${_gcc_bin_dir}" DIRECTORY)
    set_target_properties(Sandbox PROPERTIES
        BUILD_RPATH "${_gcc_root}/lib64"
        INSTALL_RPATH "${_gcc_root}/lib64"
    )
endif()

gse_copy_runtime_deps(Sandbox)

