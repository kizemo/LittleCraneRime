@echo off
setlocal
cd /d "%~dp0.."
call env.bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
call build.bat boost
exit /b %ERRORLEVEL%
