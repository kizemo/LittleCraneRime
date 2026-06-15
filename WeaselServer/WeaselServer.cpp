// WeaselServer.cpp : main source file for WeaselServer.exe
//
//	WTL MessageLoop 封装了消息循环. 实现了 getmessage/dispatchmessage....

#include "stdafx.h"
#include "resource.h"
#include "WeaselService.h"
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <WeaselTrayUtil.h>
#include <WeaselFileLog.h>
#include <winsparkle.h>
#include <functional>
#include <ShellScalingApi.h>
#include <WinUser.h>
#include <memory>
#include <atlstr.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int nCmdShow) {
  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  if (!IsWindowsBlueOrLaterEx()) {
    CString info, cap;
    info.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING);
    cap.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING_CAPTION);
    MessageBoxExW(NULL, info, cap, MB_ICONERROR, langId);
    return 0;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // 防止服务进程开启输入法
  ImmDisableIME(-1);

  WCHAR user_name[20] = {0};
  DWORD size = _countof(user_name);
  GetUserName(user_name, &size);
  if (!_wcsicmp(user_name, L"SYSTEM")) {
    return 1;
  }

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  if (!wcscmp(L"/userdir", lpstrCmdLine)) {
    CreateDirectory(WeaselUserDataPath().c_str(), NULL);
    WeaselServerApp::explore(WeaselUserDataPath());
    return 0;
  }
  if (!wcscmp(L"/weaseldir", lpstrCmdLine)) {
    WeaselServerApp::explore(WeaselServerApp::install_dir());
    return 0;
  }
  if (!wcscmp(L"/purge-tray", lpstrCmdLine)) {
    weasel_tray::PurgeAllLegacyWeaselTrayIcons();
    WeaselAppendLogW(L"weasel.server.log", L"startup", L"purge-tray done");
    return 0;
  }
  if (!wcscmp(L"/reload-tsf", lpstrCmdLine)) {
    weasel_tray::PurgeAllLegacyWeaselTrayIcons();
    const std::wstring setup =
        WeaselServerApp::install_dir().wstring() + L"\\WeaselSetup.exe";
    wchar_t cmdline[MAX_PATH * 2] = {};
    swprintf_s(cmdline, L"\"%s\" /reload-tsf-gentle", setup.c_str());
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
      WeaselAppendLogW(L"weasel.server.log", L"startup",
                       L"reload-tsf CreateProcess failed");
      return 1;
    }
    DWORD wait = WaitForSingleObject(pi.hProcess, 60000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (wait != WAIT_OBJECT_0) {
      WeaselAppendLogW(L"weasel.server.log", L"startup",
                       L"reload-tsf wait timeout");
      return 2;
    }
    if (exit_code != 0) {
      wchar_t buf[64];
      swprintf_s(buf, L"reload-tsf gentle exit=%lu", exit_code);
      WeaselAppendLogW(L"weasel.server.log", L"startup", buf);
      return static_cast<int>(exit_code);
    }
    WeaselAppendLogW(L"weasel.server.log", L"startup", L"reload-tsf done");
    return 0;
  }
  if (!wcscmp(L"/verifyipc", lpstrCmdLine)) {
    weasel::Client client;
    if (!client.Connect()) {
      WeaselAppendLogW(L"weasel.server.log", L"verify", L"verifyipc connect failed");
      return 1;
    }
    if (!client.EnsureSession() || client.SessionId() == 0) {
      wchar_t buf[96];
      swprintf_s(buf, L"verifyipc EnsureSession failed session=%lu",
                 static_cast<unsigned long>(client.SessionId()));
      WeaselAppendLogW(L"weasel.server.log", L"verify", buf);
      return 2;
    }
    wchar_t buf[96];
    swprintf_s(buf, L"verifyipc ok session=%lu echo=%d",
               static_cast<unsigned long>(client.SessionId()),
               client.Echo() ? 1 : 0);
    WeaselAppendLogW(L"weasel.server.log", L"verify", buf);
    return client.Echo() ? 0 : 3;
  }
  if (!wcscmp(L"/ascii", lpstrCmdLine) || !wcscmp(L"/nascii", lpstrCmdLine)) {
    weasel::Client client;
    bool ascii = !wcscmp(L"/ascii", lpstrCmdLine);
    if (client.Connect())  // try to connect to running server
    {
      client.StartSession();
      client.SetAsciiMode(ascii);
    }
    return 0;
  }

  // command line option /q stops the running server
  bool quit = !wcscmp(L"/q", lpstrCmdLine) || !wcscmp(L"/quit", lpstrCmdLine);
  // restart if already running
  {
    weasel::Client client;
    if (client.Connect())  // try to connect to running server
    {
      client.ShutdownServer();
      if (quit)
        return 0;
      int retry = 0;
      while (client.Connect() && retry < 10) {
        client.ShutdownServer();
        retry++;
        Sleep(50);
      }
      if (retry >= 10)
        return 0;
    } else if (quit)
      return 0;
  }

  bool check_updates = !wcscmp(L"/update", lpstrCmdLine);
  if (check_updates) {
    WeaselServerApp::check_update();
  }

  CreateDirectory(WeaselUserDataPath().c_str(), NULL);

  int nRet = 0;
  try {
    WeaselServerApp app;
    RegisterApplicationRestart(NULL, 0);
    nRet = app.Run();
  } catch (...) {
    // bad luck...
    nRet = -1;
  }

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
