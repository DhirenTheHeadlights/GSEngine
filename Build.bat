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

REM 2. Define the main build directory and the sub-build directory
set "MAIN_BUILD_DIR=build"
set "SUB_BUILD_DIR=build"

REM 3. Combine them to form the full path to the sub-build directory
set "FULL_BUILD_DIR=%MAIN_BUILD_DIR%\%SUB_BUILD_DIR%"

REM 4. Optionally delete the sub-build directory based on a parameter or condition
set "CLEAN_BUILD=false"  REM Change to true to enable cleaning

if "%CLEAN_BUILD%"=="true" (
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

REM 5. Create the main build directory if it doesn't exist
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

REM 6. Create the sub-build directory inside the main build directory
if "%CLEAN_BUILD%"=="true" (
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
        echo Creating sub-build directory: %FULL_BUILD_DIR%"
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

REM 7. Navigate to the vcpkg directory and install msdfgen and freetype
echo =============================================================
echo Installing msdfgen and freetype via vcpkg...
echo =============================================================

REM Define the relative path to vcpkg
set "VCPKG_DIR=Engine\External\vcpkg"

REM Check if vcpkg.exe exists in the specified directory
if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo ERROR: vcpkg.exe not found in %VCPKG_DIR%
    echo Please ensure vcpkg is cloned and located at %VCPKG_DIR%
    set "ERROR_FLAG=1"
    call :handle_error
)

REM Navigate to the vcpkg directory
cd /d "%VCPKG_DIR%"

REM Bootstrap vcpkg if not already done
if not exist "vcpkg.exe" (
    echo Bootstrapping vcpkg...
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo ERROR: Failed to bootstrap vcpkg.
        set "ERROR_FLAG=1"
        call :handle_error
    )
) else (
    echo vcpkg is already bootstrapped.
)

REM All dependencies
echo Installing msdfgen and freetype for x64-windows...
vcpkg.exe install msdfgen:x64-windows freetype:x64-windows
if errorlevel 1 (
    echo ERROR: Failed to install msdfgen and/or freetype via vcpkg.
    set "ERROR_FLAG=1"
    call :handle_error
)

REM Verify installation
echo Verifying installation of msdfgen and freetype...
vcpkg.exe list | findstr /i "msdfgen freetype"
if errorlevel 1 (
    echo ERROR: msdfgen and/or freetype were not installed successfully.
    set "ERROR_FLAG=1"
    call :handle_error
) else (
    echo msdfgen and freetype installed successfully.
)

echo.

REM 8. Navigate back to the original directory (GoonSquad folder)
cd /d "%~dp0"

REM 9. Navigate to the sub-build directory
cd /d "%FULL_BUILD_DIR%"
if errorlevel 1 (
    echo ERROR: Failed to navigate to sub-build directory.
    set "ERROR_FLAG=1"
    call :handle_error
)

REM 10. Run CMake to configure the project
echo =============================================================
echo Configuring the CMake project...
echo =============================================================

REM Define the path to the vcpkg toolchain file relative to the build directory
set "CMAKE_TOOLCHAIN_FILE=Engine\External\vcpkg\scripts\buildsystems\vcpkg.cmake"

REM Run CMake configuration
cmake -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% ^
      ../../
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    set "ERROR_FLAG=1"
    call :handle_error
) else (
    echo CMake configuration completed successfully.
)

echo.

REM 11. Navigate back to the root directory
cd /d "%~dp0"

REM 12. Indicate that configuration is finished
echo =============================================================
echo Configuration finished successfully!
echo =============================================================
echo.
pause

REM =============================================================
REM Error Handling Function
REM =============================================================
:handle_error
if !ERROR_FLAG! equ 1 (
    echo =============================================================
    echo ERROR: An unexpected error occurred.
    echo =============================================================
    echo.
    pause
    exit /b 1
)
exit /b 0
