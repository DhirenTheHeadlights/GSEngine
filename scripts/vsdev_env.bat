@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vsdev_env.bat: vswhere not found at "%VSWHERE%" 1>&2
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo vsdev_env.bat: no VS install with VC tools found 1>&2
  exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
