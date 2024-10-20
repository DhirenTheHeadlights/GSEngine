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

REM Run CMake to configure the project
cmake ..

REM Run the build using CMake
cmake --build . --config Release

REM Create the output directory
echo Creating out directory...
mkdir ..\%OUT_DIR%

REM Move the built files to the output directory (adjust as needed)
echo Moving built files to the out directory...
move *.exe ..\%OUT_DIR%\
move *.dll ..\%OUT_DIR%\
move *.lib ..\%OUT_DIR%\
move *.pdb ..\%OUT_DIR%\

REM Navigate back to the root directory
cd ..

@echo Build and organization finished!
