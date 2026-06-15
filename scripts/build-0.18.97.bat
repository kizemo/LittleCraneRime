@echo off

setlocal

cd /d "%~dp0.."

call env.bat

set RELEASE_BUILD=1

set VERSION_MAJOR=0

set VERSION_MINOR=18

set VERSION_PATCH=97

set WEASEL_BUILD=0
set TARGET_RELEASE_DIR=F:\soft\02office\rime\release

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

call build.bat weasel

if errorlevel 1 exit /b 1

call build.bat installer

if errorlevel 1 exit /b 1

if not exist "%TARGET_RELEASE_DIR%" mkdir "%TARGET_RELEASE_DIR%"

copy /Y "F:\soft\02office\rimetrae\release\weasel-0.18.97.0-installer.exe" "%TARGET_RELEASE_DIR%\weasel-0.18.97.0-installer.exe" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\appcast.xml" "%TARGET_RELEASE_DIR%\appcast.xml" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\release-notes.html" "%TARGET_RELEASE_DIR%\release-notes.html" >nul

exit /b %ERRORLEVEL%