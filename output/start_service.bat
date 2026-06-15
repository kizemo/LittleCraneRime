@echo off
cd /d "%~dp0"
REM Do not stack WeaselServer processes (ghost tray icons / stale quick bar).
WeaselServer.exe /q >nul 2>&1
ping 127.0.0.1 -n 2 >nul
start "" WeaselServer.exe
