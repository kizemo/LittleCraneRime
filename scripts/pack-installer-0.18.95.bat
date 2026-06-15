@echo off
setlocal
cd /d "F:\soft\02office\rimetrae\weasel"
if not exist "..\release" mkdir "..\release"
powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\generate-appcast.ps1"
"%ProgramFiles(x86)%\NSIS\Bin\makensis.exe" /DWEASEL_VERSION=0.18.95 /DWEASEL_BUILD=0 /DPRODUCT_VERSION=0.18.95.0 output\install.nsi
echo MAKENSIS_EXIT=%ERRORLEVEL%
exit /b %ERRORLEVEL%
