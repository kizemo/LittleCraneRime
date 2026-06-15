@echo off
setlocal
cd /d "%~dp0..\deps\boost_1_84_0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist b2.exe call bootstrap.bat vc143
if errorlevel 1 exit /b 1
if not exist user-config.jam (
  echo using msvc : 14.3 : : ^<setup^>"C:/Program Files (x86^)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvarsall.bat" ^<rewrite-setup-scripts^>off ; > user-config.jam
)
set B2_ARGS=--user-config=user-config.jam --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread define=BOOST_USE_WINAPI_VERSION=0x0603 toolset=msvc-14.3 link=static runtime-link=static architecture=x86 release
b2 -a -j%NUMBER_OF_PROCESSORS% %B2_ARGS% address-model=64 stage
if errorlevel 1 exit /b 1
b2 -a -j%NUMBER_OF_PROCESSORS% %B2_ARGS% address-model=32 stage
exit /b %ERRORLEVEL%
