@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\weasel_build
call env.bat
if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=18
if not defined VERSION_PATCH set VERSION_PATCH=0
if not defined WEASEL_VERSION set WEASEL_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
if not defined WEASEL_BUILD set WEASEL_BUILD=0
if not defined PRODUCT_VERSION set PRODUCT_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%
if not defined FILE_VERSION set FILE_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%
set PATH=%DEVTOOLS_PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin;%PATH%
cscript.exe //Nologo render.js weasel.props BOOST_ROOT PLATFORM_TOOLSET VERSION_MAJOR VERSION_MINOR VERSION_PATCH PRODUCT_VERSION FILE_VERSION
set MSB="%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
%MSB% weasel.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 exit /b 1
%MSB% weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m
if errorlevel 1 exit /b 1
if not exist release mkdir release
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content 'update\appcast.xml' -Raw) -replace 'weasel-[0-9.]+-installer.exe', 'weasel-%PRODUCT_VERSION%-installer.exe' -replace 'sparkle:version=\"[0-9.]+\"', 'sparkle:version=\"%WEASEL_VERSION%\"' | Set-Content 'release\appcast.xml' -Encoding UTF8"
"%ProgramFiles(x86)%\NSIS\Bin\makensis.exe" /DWEASEL_VERSION=%WEASEL_VERSION% /DWEASEL_BUILD=%WEASEL_BUILD% /DPRODUCT_VERSION=%PRODUCT_VERSION% output\install.nsi
exit /b %ERRORLEVEL%
