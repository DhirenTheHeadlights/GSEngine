set(SDK_ROOT "")
set(NVPERF_INCLUDE_DIR "")
if(DEFINED ENV{NSIGHT_PERF_SDK} AND NOT "$ENV{NSIGHT_PERF_SDK}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{NSIGHT_PERF_SDK}" SDK_ROOT)
    file(GLOB_RECURSE HOST_HEADER_CANDIDATES "${SDK_ROOT}/nvperf_host.h" "${SDK_ROOT}/*/nvperf_host.h")
    if(HOST_HEADER_CANDIDATES)
        list(GET HOST_HEADER_CANDIDATES 0 HOST_HEADER_PATH)
        get_filename_component(NVPERF_INCLUDE_DIR "${HOST_HEADER_PATH}" DIRECTORY)
    else()
        message(WARNING
            "NSIGHT_PERF_SDK is set but no nvperf_host.h was found under it:\n"
            "  Got:      ${SDK_ROOT}\n"
            "  Expected: the unzipped Nsight Perf SDK root, which contains an include "
            "directory holding nvperf_host.h\n"
            "Installing an empty stub; the engine will compile without GPU hardware counters.")
        set(SDK_ROOT "")
    endif()
endif()

if(SDK_ROOT STREQUAL "")
    message(STATUS
        "nsight-perf: NSIGHT_PERF_SDK not set; installing stub. "
        "Set the env var to enable in-process GPU hardware counters.")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/${PORT}")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright"
        "Stub package; the real NVIDIA Nsight Perf SDK is not installed.\n"
        "Set the NSIGHT_PERF_SDK environment variable to enable.\n")
    return()
endif()

file(INSTALL "${NVPERF_INCLUDE_DIR}/"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/nsight-perf"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

file(GLOB_RECURSE UTILITY_HEADER_CANDIDATES "${SDK_ROOT}/*/NvPerfPeriodicSamplerGpu.h")
if(UTILITY_HEADER_CANDIDATES)
    list(GET UTILITY_HEADER_CANDIDATES 0 UTILITY_HEADER_PATH)
    get_filename_component(UTILITY_INCLUDE_DIR "${UTILITY_HEADER_PATH}" DIRECTORY)
    file(INSTALL "${UTILITY_INCLUDE_DIR}/"
        DESTINATION "${CURRENT_PACKAGES_DIR}/include/nsight-perf-utility"
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
else()
    message(STATUS
        "nsight-perf: NvPerfUtility headers not found under ${SDK_ROOT}; "
        "only the core NvPerf C API will be available.")
endif()

set(HOST_DLL_NAME "nvperf_grfx_host.dll")
file(GLOB_RECURSE HOST_DLL_CANDIDATES "${SDK_ROOT}/*/${HOST_DLL_NAME}")
if(NOT HOST_DLL_CANDIDATES)
    message(FATAL_ERROR
        "Expected ${HOST_DLL_NAME} somewhere under ${SDK_ROOT}. "
        "The SDK layout changed; update this port.")
endif()
list(GET HOST_DLL_CANDIDATES 0 HOST_DLL_PATH)

file(INSTALL "${HOST_DLL_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
if(NOT VCPKG_BUILD_TYPE STREQUAL "release")
    file(INSTALL "${HOST_DLL_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

set(CONFIG_DIR "${CURRENT_PACKAGES_DIR}/share/unofficial-${PORT}")
file(MAKE_DIRECTORY "${CONFIG_DIR}")

file(WRITE "${CONFIG_DIR}/unofficial-nsight-perf-config.cmake" "
if(NOT TARGET unofficial::nsight-perf::nsight-perf)
    get_filename_component(_NSIGHT_PERF_PREFIX \"\${CMAKE_CURRENT_LIST_DIR}/../..\" ABSOLUTE)

    add_library(unofficial::nsight-perf::nsight-perf INTERFACE IMPORTED)
    set_target_properties(unofficial::nsight-perf::nsight-perf PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${_NSIGHT_PERF_PREFIX}/include/nsight-perf;\${_NSIGHT_PERF_PREFIX}/include/nsight-perf-utility\"
    )
endif()
")

set(COPYRIGHT_CANDIDATES
    "${SDK_ROOT}/EULA.txt"
    "${SDK_ROOT}/LICENSE.txt"
    "${SDK_ROOT}/LICENSE"
    "${SDK_ROOT}/license.txt"
)
set(COPYRIGHT_FOUND OFF)
foreach(CANDIDATE IN LISTS COPYRIGHT_CANDIDATES)
    if(EXISTS "${CANDIDATE}")
        vcpkg_install_copyright(FILE_LIST "${CANDIDATE}")
        set(COPYRIGHT_FOUND ON)
        break()
    endif()
endforeach()
if(NOT COPYRIGHT_FOUND)
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright"
        "Refer to the NVIDIA Nsight Perf SDK End User License Agreement bundled with your SDK download.\n")
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/usage"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage"
    COPYONLY
)
