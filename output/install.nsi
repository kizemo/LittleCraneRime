; weasel installation script
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include x64.nsh
!include winVer.nsh
!include "Win\RestartManager.nsh"

Unicode true

;--------------------------------
; General

!ifndef WEASEL_VERSION
!define WEASEL_VERSION 0.1.0
!endif

!ifndef WEASEL_BUILD
!define WEASEL_BUILD 0
!endif

!define WEASEL_ROOT $INSTDIR\weasel-${WEASEL_VERSION}
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel"

; The name of the installer
Name "小鹅 ${WEASEL_VERSION}"

; The file to write
OutFile "..\..\release\weasel-${PRODUCT_VERSION}-installer.exe"

VIProductVersion "${WEASEL_VERSION}.${WEASEL_BUILD}"
VIAddVersionKey /LANG=2052 "ProductName" "小鹅"
VIAddVersionKey /LANG=2052 "Comments" "Powered by RIME | 中州韻輸入法引擎"
VIAddVersionKey /LANG=2052 "CompanyName" "式恕堂"
VIAddVersionKey /LANG=2052 "LegalCopyright" "Copyleft RIME Developers"
VIAddVersionKey /LANG=2052 "FileDescription" "小鹅輸入法"
VIAddVersionKey /LANG=2052 "FileVersion" "${WEASEL_VERSION}"

!define MUI_ICON ..\resource\weasel.ico
SetCompressor /SOLID lzma


; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------
; Modern UI (candidate-bar inspired dark chrome)

!define MUI_HEADERBGCOLOR 1C1C1E
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_CUSTOMFUNCTION_GUIINIT weaselGuiInit

!define MUI_BGCOLOR 1C1C1E
!define MUI_TEXTCOLOR FFFFFF
!define MUI_INSTFILESPAGE_COLORS "1C1C1E FFFFFF"
!define MUI_FINISHPAGE_TITLE "安装完成"
!define MUI_FINISHPAGE_TEXT "小鹅输入法已安装。$\r$\n$\r$\n安装程序已自动完成：更新 System32 DLL、部署配置、重启 TSF 输入法服务并启动服务。$\r$\n可直接开始使用，无需注销、重启或手动重新部署。$\r$\n若个别应用仍显示旧输入法，关闭该应用后重新打开即可。"

;--------------------------------

; Pages

!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------

; Languages

!insertmacro MUI_LANGUAGE "TradChinese"
LangString DISPLAYNAME ${LANG_TRADCHINESE} "小鵝輸入法"
LangString LNKFORMANUAL ${LANG_TRADCHINESE} "【小鵝】說明書"
LangString LNKFORSETTING ${LANG_TRADCHINESE} "【小鵝】輸入法設定"
LangString LNKFORDICT ${LANG_TRADCHINESE} "【小鵝】用戶詞典管理"
LangString LNKFORSYNC ${LANG_TRADCHINESE} "【小鵝】用戶資料同步"
LangString LNKFORDEPLOY ${LANG_TRADCHINESE} "【小鵝】重新部署"
LangString LNKFORSERVER ${LANG_TRADCHINESE} "小鵝算法服務"
LangString LNKFORUSERFOLDER ${LANG_TRADCHINESE} "【小鵝】用戶文件夾"
LangString LNKFORAPPFOLDER ${LANG_TRADCHINESE} "【小鵝】程序文件夾"
LangString LNKFORUPDATER ${LANG_TRADCHINESE} "【小鵝】檢查新版本"
LangString LNKFORSETUP ${LANG_TRADCHINESE} "【小鵝】安裝選項"
LangString LNKFORUNINSTALL ${LANG_TRADCHINESE} "卸載小鵝"
LangString CONFIRMATION ${LANG_TRADCHINESE} "安裝前，請先卸載舊版本的小鵝。$\n$\n按下「確定」移除舊版本，按下「取消」放棄本次安裝。"
LangString SYSTEMVERSIONNOTOK ${LANG_TRADCHINESE} "您的系统不被支持，最低系統要求:Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_TRADCHINESE} "自動檢查版本更新？"

!insertmacro MUI_LANGUAGE "SimpChinese"
LangString DISPLAYNAME ${LANG_SIMPCHINESE} "小鹅输入法"
LangString LNKFORMANUAL ${LANG_SIMPCHINESE} "【小鹅】说明书"
LangString LNKFORSETTING ${LANG_SIMPCHINESE} "【小鹅】输入法设定"
LangString LNKFORDICT ${LANG_SIMPCHINESE} "【小鹅】用户词典管理"
LangString LNKFORSYNC ${LANG_SIMPCHINESE} "【小鹅】用户资料同步"
LangString LNKFORDEPLOY ${LANG_SIMPCHINESE} "【小鹅】重新部署"
LangString LNKFORSERVER ${LANG_SIMPCHINESE} "小鹅算法服务"
LangString LNKFORUSERFOLDER ${LANG_SIMPCHINESE} "【小鹅】用户文件夹"
LangString LNKFORAPPFOLDER ${LANG_SIMPCHINESE} "【小鹅】程序文件夹"
LangString LNKFORUPDATER ${LANG_SIMPCHINESE} "【小鹅】检查新版本"
LangString LNKFORSETUP ${LANG_SIMPCHINESE} "【小鹅】安装选项"
LangString LNKFORUNINSTALL ${LANG_SIMPCHINESE} "卸载小鹅"
LangString CONFIRMATION ${LANG_SIMPCHINESE} '安装前，请先卸载旧版本的小鹅。$\n$\n点击 "确定" 移除旧版本，或点击 "取消" 放弃本次安装。'
LangString SYSTEMVERSIONNOTOK ${LANG_SIMPCHINESE} "您的系統不被支持，最低系统要求:Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_SIMPCHINESE} "自动检查版本更新？"

!insertmacro MUI_LANGUAGE "English"
LangString DISPLAYNAME ${LANG_ENGLISH} "Weasel"
LangString LNKFORMANUAL ${LANG_ENGLISH} "Weasel Manual"
LangString LNKFORSETTING ${LANG_ENGLISH} "Weasel Settings"
LangString LNKFORDICT ${LANG_ENGLISH} "Weasel Dictionary Manager"
LangString LNKFORSYNC ${LANG_ENGLISH} "Weasel Sync User Profile"
LangString LNKFORDEPLOY ${LANG_ENGLISH} "Weasel Deploy"
LangString LNKFORSERVER ${LANG_ENGLISH} "Weasel Server"
LangString LNKFORUSERFOLDER ${LANG_ENGLISH} "Weasel User Folder"
LangString LNKFORAPPFOLDER ${LANG_ENGLISH} "Weasel App Folder"
LangString LNKFORUPDATER ${LANG_ENGLISH} "Weasel Check for Updates"
LangString LNKFORSETUP ${LANG_ENGLISH} "Weasel Installation Preference"
LangString LNKFORUNINSTALL ${LANG_ENGLISH} "Uninstall Weasel"
LangString CONFIRMATION ${LANG_ENGLISH} "Before installation, please uninstall the old version of Weasel.$\n$\nPress 'OK' to remove the old version, or 'Cancel' to abort installation."
LangString SYSTEMVERSIONNOTOK ${LANG_ENGLISH} "Your system not supported, minimium system required: Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_ENGLISH} "Automatically check for updates?"

;--------------------------------

Function weaselGuiInit
  ; Dark title bar (DWMWA_USE_IMMERSIVE_DARK_MODE = 20)
  System::Call 'dwmapi::DwmSetWindowAttribute(i $HWNDPARENT, i 20, i *1, i 4)'
FunctionEnd

Function .onInit
  ; if not version >= 8.1, quit and MessageBox(if not silent)
  ${IfNot} ${AtLeastWin8.1}
    IfSilent toquit
    MessageBox MB_OK '$(SYSTEMVERSIONNOTOK)'
toquit:
    Quit
  ${EndIf}

  ReadRegStr $R0 HKLM "Software\Rime\Weasel" "InstallDir"
  StrCmp $R0 "" 0 skip
  ; The default installation directory
  ; install x64 build for NativeARM64_WINDOWS11 and NativeAMD64_WINDOWS11
  ${If} ${AtLeastWin11} ; Windows 11 and above
    ${If} ${IsNativeARM64}
      StrCpy $INSTDIR "$PROGRAMFILES64\Rime"
    ${ElseIf} ${IsNativeAMD64}
      StrCpy $INSTDIR "$PROGRAMFILES64\Rime"
    ${Else}
      StrCpy $INSTDIR "$PROGRAMFILES\Rime"
    ${Endif}
  ; install x64 build for NativeAMD64_BELLOW_WINDOWS11
  ${Else} ; Windows 10 or bellow
    ${If} ${IsNativeAMD64}
      StrCpy $INSTDIR "$PROGRAMFILES64\Rime"
    ${Else}
      StrCpy $INSTDIR "$PROGRAMFILES\Rime"
    ${Endif}
  ${Endif}
skip:
  ; 终止所有路径下正在运行的 WeaselServer，避免旧版继续占用托盘与 Run 键
  ExecWait 'cmd.exe /c taskkill /f /im WeaselServer.exe 2>nul'

  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel" \
  "UninstallString"
  StrCmp $R0 "" done

  ; 已安装旧版：直接覆盖升级，不卸载
  ReadRegStr $R1 HKLM SOFTWARE\Rime\Weasel "WeaselRoot"
  StrCmp $R1 "" done
  IfFileExists $R1\data\*.* 0 +3
  CreateDirectory $TEMP\weasel-backup
  CopyFiles /SILENT $R1\data\*.* $TEMP\weasel-backup
  ExecWait '"$R1\WeaselServer.exe" /quit'

done:
FunctionEnd

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Rime\Weasel" "InstallDir"

; The stuff to install
Section "Weasel"

  SectionIn RO

  ; Write the new installation path into the registry
  ; redirect on 64 bit system
  ; HKLM SOFTWARE\WOW6432Node\Rime\Weasel "InstallDir" "$INSTDIR"
  WriteRegStr HKLM SOFTWARE\Rime\Weasel "InstallDir" "$INSTDIR"

  ; Reset INSTDIR for the new version
  StrCpy $INSTDIR "${WEASEL_ROOT}"

  IfFileExists "$INSTDIR\WeaselServer.exe" 0 +2
  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'

  ; 0.18.99: 加强进程强制结束 + Windows Restart Manager，确保 DLL/IME 文件不被占用
  ; 阶段 1: taskkill 强制结束所有 Weasel* 进程（包括嵌套子进程）
  nsExec::ExecToLog 'taskkill /f /im WeaselServer.exe /t'
  nsExec::ExecToLog 'taskkill /f /im WeaselDeployer.exe /t'
  nsExec::ExecToLog 'taskkill /f /im WeaselSetup.exe /t'
  nsExec::ExecToLog 'taskkill /f /im WeaselVerify.exe /t'
  ; 阶段 2: Windows Restart Manager API 关闭持有目标文件句柄的所有进程
  ; (RmForceShutdown 即使进程无响应也能终止，是 taskkill 的强力补充)
  !insertmacro RestartManager_ShutdownFile "$INSTDIR\WeaselServer.exe" $0
  !insertmacro RestartManager_ShutdownFile "$INSTDIR\WeaselDeployer.exe" $0
  !insertmacro RestartManager_ShutdownFile "$INSTDIR\rime.dll" $0
  !insertmacro RestartManager_ShutdownFile "$INSTDIR\WinSparkle.dll" $0
  ${If} ${RunningX64}
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\weaselx64.dll" $0
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\weaselx64.ime" $0
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\Win32\WeaselServer.exe" $0
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\Win32\WeaselDeployer.exe" $0
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\Win32\rime.dll" $0
    !insertmacro RestartManager_ShutdownFile "$INSTDIR\Win32\WinSparkle.dll" $0
  ${EndIf}
  ; 阶段 3: 等待进程完全退出 (DLL 句柄需要时间释放)
  Sleep 3000

  SetOverwrite try
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; 0.18.99: 显式删除旧版二进制，避免大小一致但内容不同时 NSIS 跳过覆盖
  ; /REBOOTOK: 若文件仍被占用 (如 TSF 持有 weasel.ime)，标记为下次启动时删除
  Delete /REBOOTOK "$INSTDIR\weasel.dll"
  Delete /REBOOTOK "$INSTDIR\weaselx64.dll"
  Delete /REBOOTOK "$INSTDIR\weasel.ime"
  Delete /REBOOTOK "$INSTDIR\weaselx64.ime"
  Delete /REBOOTOK "$INSTDIR\WeaselServer.exe"
  Delete /REBOOTOK "$INSTDIR\WeaselDeployer.exe"
  Delete /REBOOTOK "$INSTDIR\WeaselSetup.exe"
  Delete /REBOOTOK "$INSTDIR\WeaselVerify.exe"
  Delete /REBOOTOK "$INSTDIR\rime.dll"
  Delete /REBOOTOK "$INSTDIR\WinSparkle.dll"
  Delete /REBOOTOK "$INSTDIR\Win32\WeaselServer.exe"
  Delete /REBOOTOK "$INSTDIR\Win32\WeaselDeployer.exe"
  Delete /REBOOTOK "$INSTDIR\Win32\rime.dll"
  Delete /REBOOTOK "$INSTDIR\Win32\WinSparkle.dll"
  ; 0.18.99: 同时删除可能仍在运行的辅助工具 (若 WeaselSetup/Verify 等正在运行则重启后删除)
  Delete /REBOOTOK "$INSTDIR\7z.dll"
  Delete /REBOOTOK "$INSTDIR\7z.exe"
  Delete /REBOOTOK "$INSTDIR\curl.exe"

  IfFileExists $TEMP\weasel-backup\*.* 0 program_files
  CreateDirectory $INSTDIR\data
  CopyFiles $TEMP\weasel-backup\*.* $INSTDIR\data
  RMDir /r $TEMP\weasel-backup

program_files:
  File "LICENSE.txt"
  File "README.txt"
  File "7-zip-license.txt"
  File "7z.dll"
  File "7z.exe"
  File "COPYING-curl.txt"
  File "curl.exe"
  File "curl-ca-bundle.crt"
  File "rime-install.bat"
  File "rime-install-config.bat"
  File "start_service.bat"
  File "stop_service.bat"
  File "weasel.dll"
  ${If} ${RunningX64}
    File "weaselx64.dll"
  ${EndIf}
  ${If} ${IsNativeARM64}
    File /nonfatal "weaselARM.dll"
    File /nonfatal "weaselARM64.dll"
    File /nonfatal "weaselARM64X.dll"
  ${EndIf}
  File "weasel.ime"
  ${If} ${RunningX64}
    File "weaselx64.ime"
  ${EndIf}
  ${If} ${IsNativeARM64}
    File /nonfatal "weaselARM.ime"
    File /nonfatal "weaselARM64.ime"
    File /nonfatal "weaselARM64X.ime"
  ${EndIf}
  ; 0.18.99: 使用 /oname= 给文件唯一内部名, 避免 x86 在后覆盖 x64.
  ; 原代码多个 ${If} 块中都有 File "WeaselServer.exe" (x64) 和 File "Win32\WeaselServer.exe" (x86),
  ; NSIS 用最后调用的源覆盖前面的, 导致 installer 永远包含 x86 版本.
  ; 修复: 用 /oname= 强制指定内部名, 让 x64 和 x86 文件有不同内部路径, 同时存在于 installer.
  ${If} ${AtLeastWin11} ; Windows 11 and above
    ${If} ${IsNativeARM64}
      File /oname=WeaselServer.exe "WeaselServer.exe"
      File /oname=WeaselDeployer.exe "WeaselDeployer.exe"
      File /oname=rime.dll "rime.dll"
      File /oname=WinSparkle.dll "WinSparkle.dll"
      File /oname=Win32\WeaselServer.exe "Win32\WeaselServer.exe"
      File /oname=Win32\WeaselDeployer.exe "Win32\WeaselDeployer.exe"
      File /oname=Win32\rime.dll "Win32\rime.dll"
      File /oname=Win32\WinSparkle.dll "Win32\WinSparkle.dll"
    ${ElseIf} ${IsNativeAMD64}
      File /oname=WeaselServer.exe "WeaselServer.exe"
      File /oname=WeaselDeployer.exe "WeaselDeployer.exe"
      File /oname=rime.dll "rime.dll"
      File /oname=WinSparkle.dll "WinSparkle.dll"
      File /oname=Win32\WeaselServer.exe "Win32\WeaselServer.exe"
      File /oname=Win32\WeaselDeployer.exe "Win32\WeaselDeployer.exe"
      File /oname=Win32\rime.dll "Win32\rime.dll"
      File /oname=Win32\WinSparkle.dll "Win32\WinSparkle.dll"
    ${Else}
      File /oname=Win32\WeaselServer.exe "Win32\WeaselServer.exe"
      File /oname=Win32\WeaselDeployer.exe "Win32\WeaselDeployer.exe"
      File /oname=Win32\rime.dll "Win32\rime.dll"
      File /oname=Win32\WinSparkle.dll "Win32\WinSparkle.dll"
    ${Endif}
  ; install x64 build for NativeAMD64_BELLOW_WINDOWS11
  ${Else} ; Windows 10 or bellow
    ${If} ${IsNativeAMD64}
      File /oname=WeaselServer.exe "WeaselServer.exe"
      File /oname=WeaselDeployer.exe "WeaselDeployer.exe"
      File /oname=rime.dll "rime.dll"
      File /oname=WinSparkle.dll "WinSparkle.dll"
      File /oname=Win32\WeaselServer.exe "Win32\WeaselServer.exe"
      File /oname=Win32\WeaselDeployer.exe "Win32\WeaselDeployer.exe"
      File /oname=Win32\rime.dll "Win32\rime.dll"
      File /oname=Win32\WinSparkle.dll "Win32\WinSparkle.dll"
    ${Else}
      File /oname=Win32\WeaselServer.exe "Win32\WeaselServer.exe"
      File /oname=Win32\WeaselDeployer.exe "Win32\WeaselDeployer.exe"
      File /oname=Win32\rime.dll "Win32\rime.dll"
      File /oname=Win32\WinSparkle.dll "Win32\WinSparkle.dll"
    ${Endif}
  ${Endif}

  File "WeaselSetup.exe"
  File "WeaselVerify.exe"
  ; shared data files
  SetOutPath $INSTDIR\data
  File "data\*.yaml"
  File /nonfatal "data\*.txt"
  File /nonfatal "data\*.gram"
  ; rime-ice dictionaries and lua scripts
  SetOutPath $INSTDIR\data\cn_dicts
  File /r "data\cn_dicts\*.*"
  SetOutPath $INSTDIR\data\en_dicts
  File /r "data\en_dicts\*.*"
  SetOutPath $INSTDIR\data\lua
  File /r "data\lua\*.*"
  SetOutPath $INSTDIR\data\build
  File /nonfatal "data\build\.gitkeep"
  ; opencc data files
  SetOutPath $INSTDIR\data\opencc
  File "data\opencc\*.json"
  File /nonfatal "data\opencc\*.ocd*"
  ; images
  SetOutPath $INSTDIR\data\preview
  File "data\preview\*.png"

  SetOutPath $INSTDIR

  ; test /T flag for zh_TW locale
  StrCpy $R2 "/i"
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/S" $R1
  IfErrors +2 0
  StrCpy $R2 "/s"
  ${GetOptions} $R0 "/T" $R1
  IfErrors +2 0
  StrCpy $R2 "/t"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayName" "$(DISPLAYNAME)"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\WeaselServer.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayVersion" "${WEASEL_VERSION}.${WEASEL_BUILD}"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "Publisher" "式恕堂"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "URLInfoAbout" "https://rime.im/"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "HelpLink" "https://rime.im/docs/"
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Stop server before copying System32 weasel.dll (do not kill user apps or ctfmon)
  ExecWait 'cmd.exe /c taskkill /f /im WeaselServer.exe 2>nul'
  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'
  Sleep 1000
  ; Persist locale choice for WeaselSetup /upgrade (reads HKCU Hant)
  StrCmp $R2 "/t" 0 +3
    WriteRegDWORD HKCU "Software\Rime\Weasel" "Hant" 1
    Goto upgrade_run
  StrCmp $R2 "/s" 0 +3
    WriteRegDWORD HKCU "Software\Rime\Weasel" "Hant" 0
  upgrade_run:
  ExecWait '"$INSTDIR\WeaselSetup.exe" /upgrade' $0
  IntCmp $0 0 upgrade_ok 0 0
    MessageBox MB_OK|MB_ICONSTOP "WeaselSetup /upgrade 失败 (退出码 $0)。System32 中的 weasel.dll 未更新，请查看 %TEMP%\rime.weasel\install.log"
    Abort
  upgrade_ok:

  ; One-shot post-install: start server, deploy, reload TSF
  ExecWait '"$INSTDIR\WeaselSetup.exe" /postinstall' $R6
  IntCmp $R6 0 postinstall_ok 0 0
    MessageBox MB_OK|MB_ICONSTOP "安装收尾失败 (WeaselSetup /postinstall 退出码 $R6)。$\r$\n请查看 %TEMP%\rime.weasel\install.log"
    Abort
  postinstall_ok:

  ; Write install log summary for troubleshooting
  CreateDirectory "$TEMP\rime.weasel"
  FileOpen $9 "$TEMP\rime.weasel\install.log" a
  FileWrite $9 "Weasel ${PRODUCT_VERSION} installed to $INSTDIR$\r$\n"
  FileWrite $9 "WeaselSetup /upgrade exit=$0$\r$\n"
  FileWrite $9 "WeaselSetup /postinstall exit=$R6$\r$\n"
  ${If} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  ReadRegStr $R3 HKLM "Software\Rime\Weasel" "WeaselRoot"
  FileWrite $9 "WeaselRoot=$R3$\r$\n"
  FileWrite $9 "InstallDir=$INSTDIR$\r$\n"
  ReadRegStr $R4 HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer"
  FileWrite $9 "Run WeaselServer=$R4$\r$\n"
  FileWrite $9 "Install complete.$\r$\n"
  FileClose $9

  ; 安装完成后默认不自动检查更新（可在托盘菜单手动开启）
  WriteRegStr HKCU "Software\Rime\Weasel\Updates" "CheckForUpdates" "0"

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\$(DISPLAYNAME)"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORMANUAL).lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETTING).lnk" "$INSTDIR\WeaselDeployer.exe" "" "$SYSDIR\shell32.dll" 21
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDICT).lnk" "$INSTDIR\WeaselDeployer.exe" "/dict" "$SYSDIR\shell32.dll" 6
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSYNC).lnk" "$INSTDIR\WeaselDeployer.exe" "/sync" "$SYSDIR\shell32.dll" 26
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDEPLOY).lnk" "$INSTDIR\WeaselDeployer.exe" "/deploy" "$SYSDIR\shell32.dll" 144
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSERVER).lnk" "$INSTDIR\WeaselServer.exe" "" "$INSTDIR\WeaselServer.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUSERFOLDER).lnk" "$INSTDIR\WeaselServer.exe" "/userdir" "$SYSDIR\shell32.dll" 126
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORAPPFOLDER).lnk" "$INSTDIR\WeaselServer.exe" "/weaseldir" "$SYSDIR\shell32.dll" 19
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUPDATER).lnk" "$INSTDIR\WeaselServer.exe" "/update" "$SYSDIR\shell32.dll" 13
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETUP).lnk" "$INSTDIR\WeaselSetup.exe" "" "$SYSDIR\shell32.dll" 162
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUNINSTALL).lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'

  ExecWait '"$INSTDIR\WeaselSetup.exe" /u'

  ; Remove registry keys
  DeleteRegKey HKLM SOFTWARE\Rime
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel"
  ; don't redirect on 64 bit system for auto run setting
  ${If} ${IsNativeARM64}
    SetRegView 64
  ${ElseIf} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer"

  ; Remove files and uninstaller
  SetOutPath $TEMP
  Delete  "$INSTDIR\data\opencc\*.*"
  Delete  "$INSTDIR\data\preview\*.*"
  Delete  "$INSTDIR\data\cn_dicts\*.*"
  Delete  "$INSTDIR\data\en_dicts\*.*"
  Delete  "$INSTDIR\data\lua\*.*"
  Delete  "$INSTDIR\data\build\*.*"
  Delete  "$INSTDIR\data\*.*"
  Delete  "$INSTDIR\*.*"
  RMDir  "$INSTDIR\data\opencc"
  RMDir  "$INSTDIR\data\preview"
  RMDir  "$INSTDIR\data\cn_dicts"
  RMDir  "$INSTDIR\data\en_dicts"
  RMDir  "$INSTDIR\data\lua"
  RMDir  "$INSTDIR\data\build"
  RMDir  "$INSTDIR\data"
  RMDir  "$INSTDIR"
  SetShellVarContext all
  Delete  "$SMPROGRAMS\$(DISPLAYNAME)\*.*"
  RMDir  "$SMPROGRAMS\$(DISPLAYNAME)"

SectionEnd
