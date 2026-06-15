@echo off
setlocal
cd /d "F:\soft\02office\rimetrae\weasel"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
msbuild.exe weasel.sln /t:WeaselSetup /p:Configuration=Release /p:Platform=Win32 /v:minimal /fl /flp:logfile=F:\soft\02office\rimetrae\weasel\msbuild-setup.log;verbosity=normal
echo MSBUILD_EXIT=%ERRORLEVEL%
exit /b %ERRORLEVEL%
