@echo off
rem Configures and builds the plugin and its tests.
rem
rem The plugin builds as a subproject of libKCD2: its .buildenv/CMakeLists.txt
rem globs Projects/*/.buildenv/CMakeLists.txt, which reaches this project through
rem the directory junction Projects\SavegameRenamer.
rem
rem Pass a target name to build just that one, e.g. build_cpp.bat SavegameRenamerTests.
setlocal

set "VCPKG_ROOT=D:\Games\Self-Mods\KCD2\_deps\vcpkg"
set "VS=D:\IDE\Microsoft Visual Studio\18\Community"
set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "RE=D:\Games\Self-Mods\KCD2\_deps\libKCD2\.buildenv"

if not exist "%CMAKE%" (
    echo cmake not found at "%CMAKE%"
    exit /b 1
)

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

"%CMAKE%" --preset release -S "%RE%"
if errorlevel 1 exit /b 1

if "%~1"=="" (
    "%CMAKE%" --build "%RE%\build-release"
) else (
    "%CMAKE%" --build "%RE%\build-release" --target %*
)
if errorlevel 1 exit /b 1

echo.
echo BUILD OK
