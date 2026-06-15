@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
echo PATH=%PATH%
where cl
cl 2>&1 | more +0
echo VCINSTALLDIR=%VCINSTALLDIR%
echo VCToolsVersion=%VCToolsVersion%
exit /b 0
