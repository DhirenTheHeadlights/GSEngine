set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
set(VCPKG_POLICY_DLLS_WITHOUT_LIBS enabled)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_ENV_PASSTHROUGH "PATH;NSIGHT_AFTERMATH_SDK;USERPROFILE")

# Build dependencies with the prebuilt gcc-trunk MinGW compiler by explicit path
# (see the toolchain file), so vcpkg doesn't depend on the gcc-trunk bin being on
# PATH during a build-triggered reconfigure.
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/gcc-trunk-mingw-toolchain.cmake")
