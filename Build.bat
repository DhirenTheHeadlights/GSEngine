@echo on
REM =============================================================
REM Batch Script to Initialize Submodules, Install Dependencies,
REM and Configure CMake Project in build/build Directory
REM =============================================================

REM 0. Ensure the script exits immediately if any command fails
setlocal enabledelayedexpansion
set "ERROR_FLAG=0"

REM 1. Initialize and update Git submodules
echo =============================================================
echo Initializing and updating Git submodules...
echo =============================================================

REM Check if .gitmodules exists
if exist ".gitmodules" (
    echo Found .gitmodules. Initializing submodules...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo ERROR: Failed to initialize and update submodules.
        set "ERROR_FLAG=1"
        call :handle_error
    ) else (
        echo Submodules initialized and updated successfully.
    )
) else (
    echo No submodules found. Skipping submodule initialization.
)

echo.

REM 2. Bootstrap vcpkg to ensure vcpkg.exe is ready
echo =============================================================
echo Bootstrapping vcpkg to ensure vcpkg.exe is available...
echo =============================================================

REM Define the relative path to vcpkg
set "VCPKG_DIR=Engine\External\vcpkg"

REM Check if vcpkg.exe exists in the specified directory
if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo vcpkg.exe not found. Bootstrapping vcpkg...
    REM Navigate to the vcpkg directory
    cd /d "%VCPKG_DIR%"
    REM Run bootstrap-vcpkg.bat
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo ERROR: Failed to bootstrap vcpkg.
        set "ERROR_FLAG=1"
        call :handle_error
    ) else (
        echo vcpkg bootstrapped successfully.
    )
    REM Navigate back to the original directory
    cd /d "%~dp0"
) else (
    echo vcpkg.exe already exists. Skipping bootstrapping.
)

echo.

REM 3. Define the main build directory and the sub-build directory
set "MAIN_BUILD_DIR=build"
set "SUB_BUILD_DIR=build"

REM 4. Combine them to form the full path to the sub-build directory
set "FULL_BUILD_DIR=%MAIN_BUILD_DIR%\%SUB_BUILD_DIR%"

REM 5. Optionally delete the sub-build directory based on a parameter or condition
set "CLEAN_BUILD=false"  REM Change to true to enable cleaning

if /i "%CLEAN_BUILD%"=="true" (
    if exist "%FULL_BUILD_DIR%" (
        echo Deleting existing sub-build directory: %FULL_BUILD_DIR%
        rmdir /s /q "%FULL_BUILD_DIR%"
        if errorlevel 1 (
            echo ERROR: Failed to delete existing sub-build directory.
            set "ERROR_FLAG=1"
            call :handle_error
        )
    ) else (
        echo Sub-build directory does not exist. No need to delete.
    )
) else (
    echo Skipping deletion of sub-build directory.
)

echo.

REM 6. Create the main build directory if it doesn't exist
if not exist "%MAIN_BUILD_DIR%" (
    echo Creating main build directory: %MAIN_BUILD_DIR%
    mkdir "%MAIN_BUILD_DIR%"
    if errorlevel 1 (
        echo ERROR: Failed to create main build directory.
        set "ERROR_FLAG=1"
        call :handle_error
    )
) else (
    echo Main build directory already exists: %MAIN_BUILD_DIR%
)

echo.

REM 7. Create the sub-build directory inside the main build directory
if /i "%CLEAN_BUILD%"=="true" (
    echo Creating sub-build directory: %FULL_BUILD_DIR%
    mkdir "%FULL_BUILD_DIR%"
    if errorlevel 1 (
        echo ERROR: Failed to create sub-build directory.
        set "ERROR_FLAG=1"
        call :handle_error
    )
) else (
    echo Ensuring sub-build directory exists: %FULL_BUILD_DIR%
    if not exist "%FULL_BUILD_DIR%" (
        echo Creating sub-build directory: %FULL_BUILD_DIR%
        mkdir "%FULL_BUILD_DIR%"
        if errorlevel 1 (
            echo ERROR: Failed to create sub-build directory.
            set "ERROR_FLAG=1"
            call :handle_error
        )
    ) else (
        echo Sub-build directory already exists: %FULL_BUILD_DIR%
    )
)

echo.

REM 8. Navigate to the vcpkg directory and install dependencies
echo =============================================================
echo Installing dependencies via vcpkg...
echo =============================================================

REM Navigate to the vcpkg directory
cd /d "%VCPKG_DIR%"

REM All dependencies
echo Installing msdfgen and freetype for x64-windows...
vcpkg.exe install msdfgen:x64-windows freetype:x64-windows vulkan:x64-windows
if errorlevel 1 (
    echo ERROR: Failed to install msdfgen and/or freetype via vcpkg.
    set "ERROR_FLAG=1"
    call :handle_error
)

REM Verify installation
echo Verifying installation of msdfgen and freetype...
vcpkg.exe list | findstr /i "msdfgen freetype"
if errorlevel 1 (
    echo ERROR: dependencies were not installed successfully.
    set "ERROR_FLAG=1"
    call :handle_error
) else (
    echo dependencies installed successfully.
)

echo.
pause
