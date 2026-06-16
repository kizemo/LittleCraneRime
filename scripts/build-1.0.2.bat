@echo off
REM Build script for weasel 1.0.2.0 (XiaoHe)
REM 版本号：1.0.2（修复 APPCAST 资源 LANGUAGE 块问题）
REM 产品名：小鹤（XiaoHe）

setlocal

cd /d "%~dp0.."

call env.bat

set RELEASE_BUILD=1
set VERSION_MAJOR=1
set VERSION_MINOR=0
set VERSION_PATCH=2
set WEASEL_BUILD=0
set PRODUCT_VERSION=1.0.2.0
set FILE_VERSION=1.0.2.0
set TARGET_RELEASE_DIR=F:\soft\02office\rime\release

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if errorlevel 1 (
  echo Failed to setup vcvars
  exit /b 1
)

call build.bat weasel
if errorlevel 1 exit /b 1

REM 0.18.99 fix: After x64 build, x86 (Win32) build was overwriting output\WeaselServer.exe
REM instead of writing to output\Win32\WeaselServer.exe. Fix: re-run x64 build to restore.
msbuild.exe weasel.sln /t:Build ^
  /p:Configuration=Release /p:Platform=x64 ^
  /m:1 /nologo /v:minimal
if errorlevel 1 exit /b 1

call build.bat installer
if errorlevel 1 exit /b 1

if not exist "%TARGET_RELEASE_DIR%" mkdir "%TARGET_RELEASE_DIR%"

copy /Y "F:\soft\02office\rimetrae\release\weasel-1.0.2.0-installer.exe" "%TARGET_RELEASE_DIR%\weasel-1.0.2.0-installer.exe" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\appcast.xml" "%TARGET_RELEASE_DIR%\appcast.xml" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\release-notes.html" "%TARGET_RELEASE_DIR%\release-notes.html" >nul

exit /b %ERRORLEVEL%
