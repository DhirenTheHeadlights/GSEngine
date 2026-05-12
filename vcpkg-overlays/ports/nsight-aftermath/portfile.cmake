set(SDK_ROOT "")
if(DEFINED ENV{NSIGHT_AFTERMATH_SDK} AND NOT "$ENV{NSIGHT_AFTERMATH_SDK}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{NSIGHT_AFTERMATH_SDK}" SDK_ROOT)
    if(NOT EXISTS "${SDK_ROOT}/include/GFSDK_Aftermath.h")
        message(WARNING
            "NSIGHT_AFTERMATH_SDK is set but does not point at a valid SDK root:\n"
            "  Got:      ${SDK_ROOT}\n"
            "  Expected: a directory containing include/GFSDK_Aftermath.h\n"
            "Installing an empty stub; the engine will compile without Aftermath support.")
        set(SDK_ROOT "")
    endif()
endif()

if(SDK_ROOT STREQUAL "")
    message(STATUS
        "nsight-aftermath: NSIGHT_AFTERMATH_SDK not set; installing stub. "
        "Set the env var to enable GPU crash dumps.")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/${PORT}")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright"
        "Stub package; the real NVIDIA Nsight Aftermath SDK is not installed.\n"
        "Set the NSIGHT_AFTERMATH_SDK environment variable to enable.\n")
    return()
endif()

file(GLOB AFTERMATH_HEADERS "${SDK_ROOT}/include/*.h")
file(INSTALL ${AFTERMATH_HEADERS} DESTINATION "${CURRENT_PACKAGES_DIR}/include/nsight-aftermath")

if(VCPKG_TARGET_IS_WINDOWS)
    set(LIB_BASENAME "GFSDK_Aftermath_Lib.x64")
    set(LIB_PATH "${SDK_ROOT}/lib/x64/${LIB_BASENAME}.lib")
    set(DLL_PATH "${SDK_ROOT}/lib/x64/${LIB_BASENAME}.dll")

    if(NOT EXISTS "${LIB_PATH}" OR NOT EXISTS "${DLL_PATH}")
        message(FATAL_ERROR
            "Expected ${LIB_BASENAME}.lib and ${LIB_BASENAME}.dll under "
            "${SDK_ROOT}/lib/x64.")
    endif()

    file(INSTALL "${LIB_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(INSTALL "${DLL_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")

    if(NOT VCPKG_BUILD_TYPE STREQUAL "release")
        file(INSTALL "${LIB_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
        file(INSTALL "${DLL_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
    endif()

    set(IMPORTED_LOCATION_REL "bin/${LIB_BASENAME}.dll")
    set(IMPORTED_IMPLIB_REL "lib/${LIB_BASENAME}.lib")
elseif(VCPKG_TARGET_IS_LINUX)
    set(LIB_BASENAME "libGFSDK_Aftermath_Lib.x64.so")
    set(SO_PATH "${SDK_ROOT}/lib/x64/${LIB_BASENAME}")

    if(NOT EXISTS "${SO_PATH}")
        message(FATAL_ERROR "Expected ${LIB_BASENAME} under ${SDK_ROOT}/lib/x64.")
    endif()

    file(INSTALL "${SO_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    if(NOT VCPKG_BUILD_TYPE STREQUAL "release")
        file(INSTALL "${SO_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
    endif()

    set(IMPORTED_LOCATION_REL "lib/${LIB_BASENAME}")
    set(IMPORTED_IMPLIB_REL "")
endif()

set(CONFIG_DIR "${CURRENT_PACKAGES_DIR}/share/unofficial-${PORT}")
file(MAKE_DIRECTORY "${CONFIG_DIR}")

set(CONFIG_FILE "${CONFIG_DIR}/unofficial-nsight-aftermath-config.cmake")
file(WRITE "${CONFIG_FILE}" "
if(NOT TARGET unofficial::nsight-aftermath::nsight-aftermath)
    get_filename_component(_AFTERMATH_PREFIX \"\${CMAKE_CURRENT_LIST_DIR}/../..\" ABSOLUTE)

    add_library(unofficial::nsight-aftermath::nsight-aftermath SHARED IMPORTED)
    set_target_properties(unofficial::nsight-aftermath::nsight-aftermath PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${_AFTERMATH_PREFIX}/include/nsight-aftermath\"
        IMPORTED_LOCATION \"\${_AFTERMATH_PREFIX}/${IMPORTED_LOCATION_REL}\"
")

if(VCPKG_TARGET_IS_WINDOWS)
    file(APPEND "${CONFIG_FILE}" "        IMPORTED_IMPLIB \"\${_AFTERMATH_PREFIX}/${IMPORTED_IMPLIB_REL}\"\n")
endif()

file(APPEND "${CONFIG_FILE}" "    )
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
        "Refer to the NVIDIA Nsight Aftermath SDK End User License Agreement bundled with your SDK download.\n")
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/usage"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage"
    COPYONLY
)
