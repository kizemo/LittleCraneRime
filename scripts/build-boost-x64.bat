@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

cd /d "%~dp0..\deps\boost_1_84_0"
if exist bin.v2 rmdir /s /q bin.v2
if exist project-config.jam del /q project-config.jam

set B2_ARGS=--user-config=user-config.jam -j%NUMBER_OF_PROCESSORS% --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread define=BOOST_USE_WINAPI_VERSION=0x0603 toolset=msvc-14.3 link=static runtime-link=static architecture=x86 address-model=64 release stage

echo PATH=%PATH%> "%~dp0..\boost-env.log"
echo VCINSTALLDIR=%VCINSTALLDIR%>> "%~dp0..\boost-env.log"
echo VCToolsInstallDir=%VCToolsInstallDir%>> "%~dp0..\boost-env.log"
where cl>> "%~dp0..\boost-env.log"

b2.exe %B2_ARGS% > "%~dp0..\boost-x64-direct.log" 2>&1
exit /b %ERRORLEVEL%
