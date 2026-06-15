@echo off
cd /d C:\weasel_build
if not exist release mkdir release
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content 'update\appcast.xml' -Raw) -replace 'weasel-[0-9.]+-installer.exe', 'weasel-0.17.4.0-installer.exe' -replace 'sparkle:version=\"[0-9.]+\"', 'sparkle:version=\"0.17.4\"' | Set-Content 'release\appcast.xml' -Encoding UTF8"
"C:\Program Files (x86)\NSIS\makensis.exe" /DWEASEL_VERSION=0.17.4 /DWEASEL_BUILD=0 /DPRODUCT_VERSION=0.17.4.0 output\install.nsi
exit /b %ERRORLEVEL%
