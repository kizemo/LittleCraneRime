@echo off
cd /d "%~dp0.."
call env.bat
set RELEASE_BUILD=1
set VERSION_MAJOR=0
set VERSION_MINOR=18
set VERSION_PATCH=45
set WEASEL_BUILD=0
set PRODUCT_VERSION=0.18.45.0
set FILE_VERSION=0.18.45.0

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

cscript.exe render.js weasel.props BOOST_ROOT PLATFORM_TOOLSET VERSION_MAJOR VERSION_MINOR VERSION_PATCH PRODUCT_VERSION FILE_VERSION

msbuild.exe weasel.sln /t:WeaselSetup /p:Configuration=Release /p:Platform=Win32
if errorlevel 1 exit /b 1
msbuild.exe weasel.sln /t:WeaselVerify /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1

if not exist ..\release mkdir ..\release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\generate-appcast.ps1
"%ProgramFiles(x86)%\NSIS\Bin\makensis.exe" ^
  /DWEASEL_VERSION=0.18.43 ^
  /DWEASEL_BUILD=0 ^
  /DPRODUCT_VERSION=0.18.43.0 ^
  output\install.nsi
exit /b %ERRORLEVEL%
