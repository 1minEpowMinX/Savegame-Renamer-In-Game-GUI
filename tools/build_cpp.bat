@echo off
rem Configures and builds the plugin and its tests.
rem
rem The plugin builds as a subproject of libKCD2: its .buildenv/CMakeLists.txt
rem globs Projects/*/.buildenv/CMakeLists.txt, which reaches this project through
rem the directory junction Projects\SavegameRenamer.
rem
rem Pass a target name to build just that one, e.g. build_cpp.bat SavegameRenamerTests.
setlocal

rem The machine paths live in build.env beside the project, which tools/buildenv.py
rem reads as well. A variable already set in the environment is left alone, so a
rem one-off run can override one without editing the file.
set "ENV_FILE=%~dp0..\build.env"
if exist "%ENV_FILE%" (
    for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%ENV_FILE%") do (
        if not defined %%A set "%%A=%%~B"
    )
)

if not defined VS_ROOT (
    echo VS_ROOT is not set: it holds the Visual Studio installation.
    echo Copy build.env.example to build.env and fill it in.
    exit /b 1
)
if not defined LIBKCD2_ROOT (
    echo LIBKCD2_ROOT is not set: it holds the libKCD2 checkout.
    echo Copy build.env.example to build.env and fill it in.
    exit /b 1
)

set "VS=%VS_ROOT%"
set "CMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "RE=%LIBKCD2_ROOT%\.buildenv"

if not exist "%CMAKE%" (
    echo cmake not found at "%CMAKE%"
    exit /b 1
)

rem Not named LINK: the MSVC linker reads a variable of that name as extra
rem input and fails looking for SavegameRenamer.obj.
rem libKCD2 finds its subprojects by globbing Projects/*/.buildenv/CMakeLists.txt,
rem so it builds only what is linked in there. Unlinked, the configure succeeds,
rem the other plugins build, and the run reports success having produced nothing
rem of ours -- which is why this is checked rather than left to be noticed.
rem Normalised, so the message names a path worth copying rather than one
rem with a ".." left in the middle of it.
for %%I in ("%~dp0..\cpp") do set "CPP_DIR=%%~fI"
set "PROJECT_LINK=%LIBKCD2_ROOT%\Projects\SavegameRenamer"
if not exist "%PROJECT_LINK%\.buildenv\CMakeLists.txt" (
    echo This project is not linked into libKCD2's Projects folder.
    echo From an elevated prompt:
    echo     mklink /J "%PROJECT_LINK%" "%CPP_DIR%"
    exit /b 1
)

rem The link may also point at a different checkout, in which case that one is
rem what would be built. Identical checkouts compare equal and are not worth
rem separating, since either produces the same plugin.
fc /b "%PROJECT_LINK%\.buildenv\CMakeLists.txt" "%CPP_DIR%\.buildenv\CMakeLists.txt" >nul 2>&1
if errorlevel 1 (
    echo "%PROJECT_LINK%" points at a different checkout of this project.
    echo Repoint it before building:
    echo     rmdir "%PROJECT_LINK%" ^&^& mklink /J "%PROJECT_LINK%" "%CPP_DIR%"
    exit /b 1
)

rem VsDevCmd.bat, which vcvars64 calls, runs "vswhere.exe" bare after changing
rem into the Installer folder. That resolves through the current directory, which
rem cmd refuses to search when NoDefaultCurrentDirectoryInExePath is set -- and
rem Visual Studio sets it for the processes it builds from, so every build inside
rem the IDE printed the failure. Putting the folder on PATH answers the call
rem without clearing a setting that is there on purpose.
set "VS_INSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if exist "%VS_INSTALLER%\vswhere.exe" set "PATH=%VS_INSTALLER%;%PATH%"

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
