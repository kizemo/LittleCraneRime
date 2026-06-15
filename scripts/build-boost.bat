@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\boost_weasel\boost_1_84_0
if not exist b2.exe call bootstrap.bat
b2 -j8 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread define=BOOST_USE_WINAPI_VERSION=0x0603 toolset=msvc-14.3 link=static runtime-link=static --build-type=complete architecture=x86 address-model=64 stage
if errorlevel 1 exit /b 1
b2 -j8 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread define=BOOST_USE_WINAPI_VERSION=0x0603 toolset=msvc-14.3 link=static runtime-link=static --build-type=complete architecture=x86 address-model=32 stage
exit /b %ERRORLEVEL%
