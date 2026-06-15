@echo off

setlocal

cd /d "%~dp0.."

call env.bat

set RELEASE_BUILD=1

set VERSION_MAJOR=0

set VERSION_MINOR=18

set VERSION_PATCH=100

set WEASEL_BUILD=0
set TARGET_RELEASE_DIR=F:\soft\02office\rime\release

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

call build.bat weasel

if errorlevel 1 exit /b 1

rem 0.18.99 fix: build.bat 中 x64 build 之后跑 x86 (Win32) build,
rem 0.18.99 之前的 project 设置中 WeaselServer.vcxproj 缺少 trailing-backslash
rem 造成 Win32 build 写 output\WeaselServer.exe 而不是 output\Win32\WeaselServer.exe,
rem 覆盖了 x64 build 的产物. NSIS 打包时读到的是 x86 版本的 WeaselServer.exe
rem (2,190,336 bytes), 不是 x64 版本的 (2,555,904 bytes).
rem 修复: 在 NSIS 打包前, 用 msbuild 仅以 x64 平台 rebuild 一次, 恢复 x64 产物.

msbuild.exe weasel.sln /t:Build ^
  /p:Configuration=Release /p:Platform=x64 ^
  /m:1 /nologo /v:minimal

if errorlevel 1 exit /b 1

call build.bat installer

if errorlevel 1 exit /b 1

if not exist "%TARGET_RELEASE_DIR%" mkdir "%TARGET_RELEASE_DIR%"

copy /Y "F:\soft\02office\rimetrae\release\weasel-0.18.100.0-installer.exe" "%TARGET_RELEASE_DIR%\weasel-0.18.100.0-installer.exe" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\appcast.xml" "%TARGET_RELEASE_DIR%\appcast.xml" >nul
copy /Y "F:\soft\02office\rimetrae\weasel\release\release-notes.html" "%TARGET_RELEASE_DIR%\release-notes.html" >nul

exit /b %ERRORLEVEL%