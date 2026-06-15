@echo off

setlocal

cd /d "%~dp0.."

call env.bat

set RELEASE_BUILD=1

set VERSION_MAJOR=0

set VERSION_MINOR=18

set VERSION_PATCH=88

set WEASEL_BUILD=0

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

call build.bat weasel

if errorlevel 1 exit /b 1

call build.bat installer

exit /b %ERRORLEVEL%
