@echo on
REM =============================================================
REM Batch Script to Initialize Submodules and Install Dependencies
REM via vcpkg
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

REM 3. Navigate to the vcpkg directory and install msdfgen and freetype
echo =============================================================
echo Installing msdfgen and freetype via vcpkg...
echo =============================================================

cd /d "%VCPKG_DIR%"

vcpkg.exe install msdfgen:x64-windows freetype:x64-windows
if errorlevel 1 (
    echo ERROR: Failed to install msdfgen and/or freetype via vcpkg.
    set "ERROR_FLAG=1"
    call :handle_error
)

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

REM 4. Navigate back to the original directory
cd /d "%~dp0"

REM 5. Indicate that configuration is finished
echo =============================================================
echo Dependencies installed successfully!
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
