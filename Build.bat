@echo off
REM Define the build and output directories
set BUILD_DIR=build
set OUT_DIR=out

REM Delete the build directory if it exists
if exist %BUILD_DIR% (
    echo Deleting existing build directory...
    rmdir /s /q %BUILD_DIR%
)

REM Delete the out directory if it exists
if exist %OUT_DIR% (
    echo Deleting existing out directory...
    rmdir /s /q %OUT_DIR%
)

REM Create the build directory
echo Creating build directory...
mkdir %BUILD_DIR%

REM Navigate to the build directory
cd %BUILD_DIR%

REM Run CMake to configure the project (but don't build)
cmake ..

REM Optional: Create the output directory (if needed later)
echo Creating out directory...
mkdir ..\%OUT_DIR%

REM Navigate back to the root directory
cd ..

@echo Configuration finished!
