@echo off
setlocal
cd /d "%~dp0.."
call env.bat
set RELEASE_BUILD=1
set VERSION_PATCH=9
set WEASEL_BUILD=0

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
  echo vcvars64 failed
  exit /b 1
)

call build.bat weasel
if errorlevel 1 exit /b 1

call build.bat installer
exit /b %ERRORLEVEL%
