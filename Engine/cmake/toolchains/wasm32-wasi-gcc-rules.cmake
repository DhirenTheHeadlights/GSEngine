# The build rules for wasm32-wasi-gcc.cmake, read after CMake's own GNU rules so these win.
#
# Every command runs with MSYS2's mingw64 bin first on PATH. cc1plus, xg++, llvm-mc,
# llvm-ar and wasm-ld are mingw binaries that load their DLLs from there, and a Ninja build
# inherits whatever PATH launched it -- which in the editor is the native toolchain's, whose
# libstdc++-6.dll is not the one LLVM was built against.
#
# A compile stops at -S and llvm-mc assembles, exactly as build_gcc_wasm.py's compile_unit
# and its link did: the driver would otherwise assemble through the build tree's `as' shim.
# Stopping at <OBJECT>.s also moves GCC's default -fdeps-file onto <OBJECT>.ddi, the scan's
# own file, which the compile would overwrite with a <OBJECT>.o target; -fdeps-file keeps
# that by-product off it.
# The link names wasm-ld directly with the line wasm_ld() in that script assembled for the
# engine, and leaves out <LINK_FLAGS>, which carries the -fcontracts the native link needs
# and wasm-ld does not take.

set(_gse_wasm_env "\"${CMAKE_COMMAND}\" -E env --modify PATH=path_list_prepend:${GSE_WASM_HOST_BIN} --")
set(_gse_wasm_assemble "${_gse_wasm_env} ${GSE_WASM_HOST_BIN}/llvm-mc.exe -triple=wasm32-unknown-unknown -mattr=+atomics,+bulk-memory -filetype=obj <OBJECT>.s -o <OBJECT>")

foreach(_lang C CXX)
    set(CMAKE_${_lang}_OUTPUT_EXTENSION .o)
    set(CMAKE_${_lang}_COMPILE_OBJECT
        "${_gse_wasm_env} <CMAKE_${_lang}_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT>.s -S <SOURCE>"
        "${_gse_wasm_assemble}")
    set(CMAKE_${_lang}_ARCHIVE_CREATE "${_gse_wasm_env} <CMAKE_AR> qc <TARGET> <OBJECTS>")
    set(CMAKE_${_lang}_ARCHIVE_APPEND "${_gse_wasm_env} <CMAKE_AR> q <TARGET> <OBJECTS>")
    set(CMAKE_${_lang}_ARCHIVE_FINISH "${_gse_wasm_env} <CMAKE_RANLIB> <TARGET>")
endforeach()

string(APPEND CMAKE_CXX_MODULE_MAP_FLAG " -fdeps-file=$out.s.ddi")

string(CONCAT CMAKE_CXX_SCANDEP_SOURCE
    "${_gse_wasm_env} <CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E -x c++ <SOURCE>"
    " -MT <DYNDEP_FILE> -MD -MF <DEP_FILE>"
    " -fmodules-ts -fdeps-file=<DYNDEP_FILE> -fdeps-target=<OBJECT> -fdeps-format=p1689r5"
    " -o <PREPROCESSED_SOURCE>")
list(APPEND CMAKE_CXX_SCANDEP_SOURCE
    "\"${CMAKE_COMMAND}\" -DDDI=<DYNDEP_FILE> -P \"${CMAKE_CURRENT_LIST_DIR}/wasm32-wasi-gcc-ddi.cmake\"")

# The engine's arm of wasm_ld(): crt1-reactor so the harness calls `entry' on a module that
# stays callable, the runtime archives in dependency order, a shared memory whose maximum is
# GSE_WASM_MAX_PAGES, and the 8 MB stack a frame of the engine needs.
math(EXPR _gse_wasm_max_memory "${GSE_WASM_MAX_PAGES} * 65536")
string(JOIN " " CMAKE_CXX_LINK_EXECUTABLE
    "${_gse_wasm_env} ${GSE_WASM_HOST_BIN}/wasm-ld.exe"
    "${GSE_WASM_SYSROOT_LIB}/crt1-reactor.o"
    "<OBJECTS> <LINK_LIBRARIES>"
    "-L${GSE_WASM_TARGET_DIR}/libstdc++-v3/src/.libs -lstdc++"
    "-L${GSE_WASM_TARGET_DIR}/libgcc -lgcc"
    "-L${GSE_WASM_SYSROOT_LIB} -lc -lwasi-emulated-signal"
    "--shared-memory --import-memory --export-memory --max-memory=${_gse_wasm_max_memory}"
    "-z stack-size=8388608"
    "--error-limit=0"
    "--allow-undefined-file=${CMAKE_CURRENT_LIST_DIR}/../wasm/host_imports.txt"
    "--export=entry"
    "--export-if-defined=__wasm_call_ctors"
    "--export-if-defined=_initialize"
    "--export-if-defined=__wasm_call_dtors"
    "-o <TARGET> --no-entry")
