// WeaselSetup.cpp : main source file for WeaselSetup.exe
//

#include "stdafx.h"

#include "resource.h"
#include "WeaselUtility.h"
#include <thread>

#include "InstallOptionsDlg.h"
#include <WeaselConstants.h>
#include <WeaselFileLog.h>

#include <fstream>
#include <tlhelp32.h>
#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

static int Run(LPTSTR lpCmdLine);
static bool IsProcAdmin();
static int RestartAsAdmin(LPTSTR lpCmdLine);

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int /*nCmdShow*/) {
  HRESULT hRes = ::CoInitialize(NULL);
  ATLASSERT(SUCCEEDED(hRes));

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  int nRet = Run(lpstrCmdLine);

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
int install(bool hant, bool silent, bool old_ime_support);
int uninstall(bool silent);
bool has_installed();
bool verify_weasel_system32_install();
int reload_tsf_gentle(bool hant);

static bool ReadHantFromRegistry() {
  bool hant = false;
  HKEY hKey = NULL;
  if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel", &hKey) ==
      ERROR_SUCCESS) {
    DWORD data = 0;
    DWORD len = sizeof(data);
    DWORD type = 0;
    if (RegQueryValueExW(hKey, L"Hant", NULL, &type, (LPBYTE)&data, &len) ==
            ERROR_SUCCESS &&
        type == REG_DWORD) {
      hant = data != 0;
    }
    RegCloseKey(hKey);
  }
  return hant;
}

static std::wstring install_dir() {
  WCHAR exe_path[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
  std::wstring dir(exe_path);
  size_t pos = dir.find_last_of(L"\\");
  dir.resize(pos);
  return dir;
}

// 若所选目录末级不是 rime，则在其下创建 rime 子目录作为用户数据目录
static std::wstring NormalizeRimeUserDir(const std::wstring& dir) {
  if (dir.empty())
    return dir;
  std::wstring path = dir;
  while (!path.empty() && (path.back() == L'\\' || path.back() == L'/'))
    path.pop_back();
  if (path.empty())
    return dir;
  const size_t pos = path.find_last_of(L"\\/");
  const std::wstring leaf =
      (pos == std::wstring::npos) ? path : path.substr(pos + 1);
  if (_wcsicmp(leaf.c_str(), L"rime") == 0)
    return path;
  return path + L"\\rime";
}

static int CustomInstall(bool installing, bool force_silent) {
  bool hant = false;
  bool silent = force_silent;
  bool old_ime_support = false;
  std::wstring user_dir;

  const WCHAR KEY[] = L"Software\\Rime\\Weasel";
  HKEY hKey;
  LSTATUS ret = RegOpenKey(HKEY_CURRENT_USER, KEY, &hKey);
  if (ret == ERROR_SUCCESS) {
    WCHAR value[MAX_PATH];
    DWORD len = sizeof(value);
    DWORD type = 0;
    DWORD data = 0;
    ret =
        RegQueryValueEx(hKey, L"RimeUserDir", NULL, &type, (LPBYTE)value, &len);
    if (ret == ERROR_SUCCESS && type == REG_SZ) {
      user_dir = value;
    }
    len = sizeof(data);
    ret = RegQueryValueEx(hKey, L"Hant", NULL, &type, (LPBYTE)&data, &len);
    if (ret == ERROR_SUCCESS && type == REG_DWORD) {
      hant = (data != 0);
      if (installing)
        silent = true;
    }
    RegCloseKey(hKey);
  }
  bool _has_installed = has_installed();
  if (!silent) {
    InstallOptionsDialog dlg;
    dlg.installed = _has_installed;
    dlg.hant = hant;
    dlg.user_dir = user_dir;
    if (IDOK != dlg.DoModal()) {
      if (!installing)
        return 1;  // aborted by user
    } else {
      hant = dlg.hant;
      user_dir = dlg.user_dir;
      old_ime_support = dlg.old_ime_support;
      _has_installed = dlg.installed;
    }
  }
  if (!_has_installed || force_silent) {
    if (0 != install(hant, silent, old_ime_support))
      return 1;
  }

  if (user_dir.empty()) {
    // default user dir %APPDATA%\Rime
    WCHAR _path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(L"%APPDATA%\\Rime", _path, _countof(_path));
    user_dir = std::wstring(_path);
  } else {
    user_dir = NormalizeRimeUserDir(user_dir);
  }
  CreateDirectoryW(user_dir.c_str(), NULL);
  ret = SetRegKeyValue(HKEY_CURRENT_USER, KEY, L"RimeUserDir", user_dir.c_str(),
                       REG_SZ, false);
  if (FAILED(HRESULT_FROM_WIN32(ret))) {
    MSG_BY_IDS(IDS_STR_ERR_WRITE_USER_DIR, IDS_STR_INSTALL_FAILED,
               MB_ICONERROR | MB_OK);
    return 1;
  }
  ret = SetRegKeyValue(HKEY_CURRENT_USER, KEY, L"Hant", (hant ? 1 : 0),
                       REG_DWORD, false);
  if (FAILED(HRESULT_FROM_WIN32(ret))) {
    MSG_BY_IDS(IDS_STR_ERR_WRITE_HANT, IDS_STR_INSTALL_FAILED,
               MB_ICONERROR | MB_OK);
    return 1;
  }
  if (_has_installed) {
    std::wstring dir(install_dir());
    std::thread th([dir]() {
      ShellExecuteW(NULL, NULL, (dir + L"\\WeaselServer.exe").c_str(), L"/q",
                    NULL, SW_SHOWNORMAL);
      Sleep(500);
      ShellExecuteW(NULL, NULL, (dir + L"\\WeaselServer.exe").c_str(), L"",
                    NULL, SW_SHOWNORMAL);
      Sleep(500);
      ShellExecuteW(NULL, NULL, (dir + L"\\WeaselDeployer.exe").c_str(),
                    L"/deploy", NULL, SW_SHOWNORMAL);
    });
    th.detach();
    if (!silent) {
      MSG_BY_IDS(IDS_STR_MODIFY_SUCCESS_INFO, IDS_STR_MODIFY_SUCCESS_CAP,
                 MB_ICONINFORMATION | MB_OK);
    }
  }

  return 0;
}

LPCTSTR GetParamByPrefix(LPCTSTR lpCmdLine, LPCTSTR prefix) {
  return (wcsncmp(lpCmdLine, prefix, wcslen(prefix)) == 0)
             ? (lpCmdLine + wcslen(prefix))
             : 0;
}

static int ReloadTsfFromInstallDir() {
  return reload_tsf_gentle(ReadHantFromRegistry());
}

static int RunChildProcess(const std::wstring& exe,
                           const wchar_t* args,
                           DWORD timeout_ms,
                           bool hidden = true) {
  wchar_t cmdline[MAX_PATH * 3] = {};
  if (args && args[0])
    swprintf_s(cmdline, L"\"%s\" %s", exe.c_str(), args);
  else
    swprintf_s(cmdline, L"\"%s\"", exe.c_str());
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  DWORD flags = hidden ? CREATE_NO_WINDOW : 0;
  if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, flags, NULL, NULL, &si,
                      &pi))
    return -1;
  WaitForSingleObject(pi.hProcess, timeout_ms);
  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return static_cast<int>(exit_code);
}

static bool StartWeaselServerDetached() {
  const std::wstring server = install_dir() + L"\\WeaselServer.exe";
  wchar_t cmdline[MAX_PATH * 2] = {};
  swprintf_s(cmdline, L"\"%s\"", server.c_str());
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
                      NULL, &si, &pi))
    return false;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

static std::wstring ReadUnicodeOrAnsiLog(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  std::string bytes((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
  if (bytes.size() >= 2 &&
      static_cast<unsigned char>(bytes[0]) == 0xFF &&
      static_cast<unsigned char>(bytes[1]) == 0xFE) {
    return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
                        (bytes.size() - 2) / sizeof(wchar_t));
  }
  return u8tow(bytes);
}

static bool WaitForTsfReady(const wchar_t* version, DWORD timeout_ms) {
  const std::wstring dll_needle = std::wstring(L"weasel.dll loaded ") + version;
  const std::wstring act_needle =
      std::wstring(L"WeaselTSF ") + version + L" ActivateEx";
  const DWORD start = GetTickCount();
  const auto log_path = WeaselLogPath() / L"weasel.tsf.log";
  while (GetTickCount() - start < timeout_ms) {
    const std::wstring content = ReadUnicodeOrAnsiLog(log_path);
    if (content.find(dll_needle) != std::wstring::npos ||
        content.find(act_needle) != std::wstring::npos)
      return true;
    Sleep(500);
  }
  return false;
}

static bool IsWeaselServerRunning();
static bool ProbeIpcSession(wchar_t* detail, size_t detail_chars);

static bool IsWeaselServerRunning() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return false;
  PROCESSENTRY32W pe = {sizeof(pe)};
  bool found = false;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"WeaselServer.exe") == 0) {
        found = true;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

// Returns the full image path of the running WeaselServer.exe, or empty string.
// Used to verify the running binary is the freshly installed one.
static std::wstring GetWeaselServerImagePath() {
  std::wstring result;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return result;
  PROCESSENTRY32W pe = {sizeof(pe)};
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"WeaselServer.exe") == 0) {
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  pe.th32ProcessID);
        if (proc) {
          wchar_t path[MAX_PATH] = {};
          DWORD size = MAX_PATH;
          if (QueryFullProcessImageNameW(proc, 0, path, &size))
            result = path;
          CloseHandle(proc);
        }
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return result;
}

// Logs old weasel-0.X.X directories in the rime install root (besides current).
// Does NOT auto-delete to avoid breaking user setups.
static void LogStaleWeaselInstalls(const std::wstring& current_dir) {
  try {
    const std::filesystem::path rime_root(L"D:\\Program Files\\Rime");
    if (!std::filesystem::exists(rime_root))
      return;
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(rime_root)) {
      if (!entry.is_directory())
        continue;
      const std::wstring name = entry.path().filename().wstring();
      if (name.size() > 7 && name.compare(0, 7, L"weasel-") == 0) {
        const std::wstring full = entry.path().wstring();
        if (_wcsicmp(full.c_str(), current_dir.c_str()) != 0) {
          ++count;
          wchar_t buf[1024] = {};
          swprintf_s(buf, L"stale install detected: %s", full.c_str());
          WeaselAppendLogW(L"install.log", L"postinstall", buf);
        }
      }
    }
    wchar_t buf[128] = {};
    swprintf_s(buf, L"stale install count=%d (not auto-removed)", count);
    WeaselAppendLogW(L"install.log", L"postinstall", buf);
  } catch (...) {
    WeaselAppendLogW(L"install.log", L"postinstall",
                     L"stale install scan exception");
  }
}

static bool ProbeIpcSession(wchar_t* detail, size_t detail_chars) {
  if (!detail || detail_chars == 0)
    return false;
  detail[0] = L'\0';
  const std::wstring server = install_dir() + L"\\WeaselServer.exe";
  for (int attempt = 0; attempt < 2; ++attempt) {
    const int rc = RunChildProcess(server.c_str(), L"/verifyipc", 15000);
    if (rc == 0) {
      swprintf_s(detail, detail_chars, L"ipc verifyipc exit=0");
      return true;
    }
    swprintf_s(detail, detail_chars, L"ipc verifyipc exit=%d attempt=%d", rc,
               attempt + 1);
    WeaselAppendLogW(L"install.log", L"postinstall", detail);
    Sleep(500);
  }
  return false;
}

static int RunUpgradeFromInstallDir(wchar_t* detail, size_t detail_chars) {
  if (!detail || detail_chars == 0)
    return 17;
  detail[0] = L'\0';
  const std::wstring setup = install_dir() + L"\\WeaselSetup.exe";
  int last_upgrade_exit = -1;
  bool saw_verify_fail = false;
  for (int attempt = 0; attempt < 3; ++attempt) {
    const int rc = RunChildProcess(setup.c_str(), L"/upgrade", 300000);
    last_upgrade_exit = rc;
    swprintf_s(detail, detail_chars, L"upgrade attempt=%d exit=%d", attempt + 1,
               rc);
    WeaselAppendLogW(L"install.log", L"postinstall", detail);
    if (rc != 0) {
      Sleep(2000);
      continue;
    }
    if (verify_weasel_system32_install())
      return 0;
    saw_verify_fail = true;
    WeaselAppendLogW(L"install.log", L"postinstall",
                     L"upgrade exit=0 but System32 verify failed, retrying");
    Sleep(2000);
  }
  if (saw_verify_fail) {
    swprintf_s(detail, detail_chars,
               L"System32 verify failed after upgrade (last exit=%d)",
               last_upgrade_exit);
    return 14;
  }
  swprintf_s(detail, detail_chars, L"upgrade failed after 3 attempts exit=%d",
             last_upgrade_exit);
  return 17;
}

static int PostInstall() {
  const std::wstring dir = install_dir();
  const std::wstring server = dir + L"\\WeaselServer.exe";
  const std::wstring deployer = dir + L"\\WeaselDeployer.exe";
  const std::wstring version = u8tow(WEASEL_VERSION);

  WeaselAppendLogW(L"install.log", L"postinstall", L"postinstall() start");

  wchar_t buf[128];
  wchar_t upgrade_detail[128] = {};
  const int upgrade_rc =
      RunUpgradeFromInstallDir(upgrade_detail, _countof(upgrade_detail));
  if (upgrade_rc != 0) {
    WeaselAppendLogW(L"install.log", L"postinstall", upgrade_detail);
    verify_weasel_system32_install();
    return upgrade_rc;
  }

  const LSTATUS run_rc = SetRegKeyValue(
      HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      L"WeaselServer", server.c_str(), REG_SZ, true);
  swprintf_s(buf, L"write Run key rc=%ld", static_cast<long>(run_rc));
  WeaselAppendLogW(L"install.log", L"postinstall", buf);
  if (run_rc != ERROR_SUCCESS)
    return 16;

  if (!StartWeaselServerDetached()) {
    WeaselAppendLogW(L"install.log", L"postinstall",
                     L"start WeaselServer failed");
    return 10;
  }
  Sleep(2500);

  int deploy_rc = 1;
  for (int attempt = 0; attempt < 5; ++attempt) {
    deploy_rc = RunChildProcess(deployer, L"/deployquiet", 600000);
    swprintf_s(buf, L"deploy attempt=%d exit=%d", attempt + 1, deploy_rc);
    WeaselAppendLogW(L"install.log", L"postinstall", buf);
    if (deploy_rc == 0)
      break;
    Sleep(2000);
  }
  if (deploy_rc != 0)
    return 11;

  const int purge_rc = RunChildProcess(server, L"/purge-tray", 60000);
  swprintf_s(buf, L"purge-tray exit=%d", purge_rc);
  WeaselAppendLogW(L"install.log", L"postinstall", buf);

  RunChildProcess(server, L"/quit", 30000);
  Sleep(500);

  if (!StartWeaselServerDetached()) {
    WeaselAppendLogW(L"install.log", L"postinstall",
                     L"restart WeaselServer failed");
    return 13;
  }
  Sleep(1000);

  const int reload_rc = ReloadTsfFromInstallDir();
  swprintf_s(buf, L"reload-tsf exit=%d", reload_rc);
  WeaselAppendLogW(L"install.log", L"postinstall", buf);
  if (reload_rc != 0)
    return 12;

  const bool verify_ok = verify_weasel_system32_install();
  swprintf_s(buf, L"verify System32 ok=%d", verify_ok ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"postinstall", buf);

  swprintf_s(buf, L"verify WeaselServer running ok=%d",
             IsWeaselServerRunning() ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"postinstall", buf);

  // Verify running WeaselServer.exe image path matches installed version
  // This catches the case where an old WeaselServer was auto-restarted
  // (e.g. via Run key or service) before the new one was started.
  {
    const std::wstring expected = server;  // server = install_dir() + L"\\WeaselServer.exe"
    const std::wstring actual = GetWeaselServerImagePath();
    wchar_t path_buf[2048] = {};
    swprintf_s(path_buf, L"verify WeaselServer image path: actual=%s expected=%s",
               actual.empty() ? L"<none>" : actual.c_str(),
               expected.c_str());
    WeaselAppendLogW(L"install.log", L"postinstall", path_buf);
    if (actual.empty() || _wcsicmp(actual.c_str(), expected.c_str()) != 0) {
      // Mismatch - kill the wrong process and restart with the right one
      WeaselAppendLogW(L"install.log", L"postinstall",
                       L"image path mismatch, forcing restart");
      // Use taskkill via cmd to ensure the process exits
      const std::wstring taskkill_cmd =
          L"cmd.exe /c taskkill /f /im WeaselServer.exe >nul 2>&1";
      RunChildProcess(L"cmd.exe", L"/c taskkill /f /im WeaselServer.exe", 10000);
      Sleep(1500);
      if (!StartWeaselServerDetached()) {
        WeaselAppendLogW(L"install.log", L"postinstall",
                         L"force-restart WeaselServer failed");
        return 16;
      }
      Sleep(1500);
      const std::wstring actual2 = GetWeaselServerImagePath();
      wchar_t recheck_buf[2048] = {};
      swprintf_s(recheck_buf, L"recheck image path: actual=%s",
                 actual2.empty() ? L"<none>" : actual2.c_str());
      WeaselAppendLogW(L"install.log", L"postinstall", recheck_buf);
    }
  }

  wchar_t ipc_detail[128] = {};
  if (ProbeIpcSession(ipc_detail, _countof(ipc_detail)))
    WeaselAppendLogW(L"install.log", L"postinstall", ipc_detail);

  // Scan and log any stale weasel-0.X.X directories so user can clean manually
  LogStaleWeaselInstalls(install_dir());

  WeaselAppendLogW(L"install.log", L"postinstall", L"postinstall() success");
  return 0;
}

static int VerifyInstall() {
  const std::wstring version = u8tow(WEASEL_VERSION);
  wchar_t buf[128];

  const bool server_running = IsWeaselServerRunning();
  swprintf_s(buf, L"verify WeaselServer running ok=%d", server_running ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"verify", buf);
  if (!server_running)
    return 18;

  wchar_t ipc_detail[128] = {};
  const bool ipc_ok = ProbeIpcSession(ipc_detail, _countof(ipc_detail));
  WeaselAppendLogW(L"install.log", L"verify", ipc_detail);
  swprintf_s(buf, L"verify ipc session ok=%d", ipc_ok ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"verify", buf);
  if (!ipc_ok)
    return 19;

  const bool verify_ok = verify_weasel_system32_install();
  swprintf_s(buf, L"verify System32 ok=%d", verify_ok ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"verify", buf);
  if (!verify_ok)
    return 14;

  const bool tsf_ok = WaitForTsfReady(version.c_str(), 8000);
  swprintf_s(buf, L"verify tsf ready %s ok=%d", version.c_str(), tsf_ok ? 1 : 0);
  WeaselAppendLogW(L"install.log", L"verify", buf);
  if (!tsf_ok)
    return 15;

  WeaselAppendLogW(L"install.log", L"verify", L"verify-install all passed");
  return 0;
}

static int Run(LPTSTR lpCmdLine) {
  constexpr bool silent = true;
  constexpr bool old_ime_support = false;
  // parameter /? or /help to show commandline args
  if (!wcscmp(L"/?", lpCmdLine) || !wcscmp(L"/help", lpCmdLine)) {
    WCHAR msg[1024] = {0};
    if (LoadString(GetModuleHandle(NULL), IDS_STR_HELP, msg,
                   sizeof(msg) / sizeof(TCHAR))) {
      MessageBox(NULL, msg, L"WeaselSetup", MB_ICONINFORMATION | MB_OK);
    } else {
      MessageBox(
          NULL,
          L"Usage: WeaselSetup.exe [options]\n"
          L"/? or /help    - Show this help message\n"
          L"/u             - Uninstall Weasel\n"
          L"/i             - Install Weasel\n"
          L"/s             - Install Weasel (Simplified Chinese)\n"
          L"/t             - Install Weasel (Traditional Chinese)\n"
          L"/ls            - Set Weasel language to Simplified Chinese\n"
          L"/lt            - Set Weasel language to Traditional Chinese\n"
          L"/le            - Set Weasel language to English\n"
          L"/eu            - Enable automatic update check\n"
          L"/du            - Disable automatic update check\n"
          L"/toggleime     - Toggle IME on open/close(ctrl+space)\n"
          L"/toggleascii   - Toggle ASCII on open/close(ctrl+space)\n"
          L"/testing       - Set update channel to testing\n"
          L"/release       - Set update channel to release\n"
          L"/upgrade       - Force reinstall TSF DLL to System32\n"
          L"/postinstall    - Start server, deploy, reload TSF (installer)\n"
          L"/verify-install - Manual diagnostics only (not used by installer)\n"
          L"/reload-tsf    - Restart TSF (same as WeaselServer.exe /reload-tsf)\n"
          L"/userdir:<dir> - Set user directory\n",
          L"WeaselSetup", MB_ICONINFORMATION | MB_OK);
    }
    return 0;
  }
  bool uninstalling = !wcscmp(L"/u", lpCmdLine);
  if (uninstalling) {
    if (IsProcAdmin())
      return uninstall(silent);
    else
      return RestartAsAdmin(lpCmdLine);
  }

  if (auto res = GetParamByPrefix(lpCmdLine, L"/userdir:")) {
    const std::wstring user_dir = NormalizeRimeUserDir(res);
    CreateDirectoryW(user_dir.c_str(), NULL);
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"RimeUserDir", user_dir.c_str(), REG_SZ);
  }

  if (!wcscmp(L"/ls", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"Language", L"chs", REG_SZ);
  } else if (!wcscmp(L"/lt", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"Language", L"cht", REG_SZ);
  } else if (!wcscmp(L"/le", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"Language", L"eng", REG_SZ);
  }

  if (!wcscmp(L"/eu", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel\\Updates",
                          L"CheckForUpdates", L"1", REG_SZ);
  }
  if (!wcscmp(L"/du", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel\\Updates",
                          L"CheckForUpdates", L"0", REG_SZ);
  }

  if (!wcscmp(L"/toggleime", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"ToggleImeOnOpenClose", L"yes", REG_SZ);
  }
  if (!wcscmp(L"/toggleascii", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"ToggleImeOnOpenClose", L"no", REG_SZ);
  }
  if (!wcscmp(L"/testing", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"UpdateChannel", L"testing", REG_SZ);
  }
  if (!wcscmp(L"/release", lpCmdLine)) {
    return SetRegKeyValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                          L"UpdateChannel", L"release", REG_SZ);
  }

  if (!IsProcAdmin()) {
    return RestartAsAdmin(lpCmdLine);
  }

  if (!wcscmp(L"/reload-tsf", lpCmdLine) ||
      !wcscmp(L"/reload-tsf-gentle", lpCmdLine)) {
    return reload_tsf_gentle(ReadHantFromRegistry());
  }

  if (!wcscmp(L"/postinstall", lpCmdLine)) {
    return PostInstall();
  }

  if (!wcscmp(L"/verify-install", lpCmdLine)) {
    return VerifyInstall();
  }

  if (!wcscmp(L"/upgrade", lpCmdLine)) {
    bool hant = false;
    HKEY hKey = NULL;
    if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel", &hKey) ==
        ERROR_SUCCESS) {
      DWORD data = 0;
      DWORD len = sizeof(data);
      DWORD type = 0;
      if (RegQueryValueExW(hKey, L"Hant", NULL, &type, (LPBYTE)&data, &len) ==
              ERROR_SUCCESS &&
          type == REG_DWORD) {
        hant = data != 0;
      }
      RegCloseKey(hKey);
    }
    return install(hant, true, old_ime_support);
  }

  bool hans = !wcscmp(L"/s", lpCmdLine);
  if (hans)
    return install(false, silent, old_ime_support);
  bool hant = !wcscmp(L"/t", lpCmdLine);
  if (hant)
    return install(true, silent, old_ime_support);
  bool installing = !wcscmp(L"/i", lpCmdLine);
  const bool setup_quiet =
      wcsstr(lpCmdLine, L"/setupquiet") != nullptr ||
      wcsstr(lpCmdLine, L"/silent") != nullptr;
  return CustomInstall(installing, setup_quiet);
}

// https://learn.microsoft.com/zh-cn/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembership
bool IsProcAdmin() {
  BOOL b = FALSE;
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID AdministratorsGroup;
  b = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                               DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &AdministratorsGroup);

  if (b) {
    if (!CheckTokenMembership(NULL, AdministratorsGroup, &b)) {
      b = FALSE;
    }
    FreeSid(AdministratorsGroup);
  }

  return (b);
}

int RestartAsAdmin(LPTSTR lpCmdLine) {
  SHELLEXECUTEINFO execInfo{0};
  TCHAR path[MAX_PATH];
  GetModuleFileName(GetModuleHandle(NULL), path, _countof(path));
  execInfo.lpFile = path;
  execInfo.lpParameters = lpCmdLine;
  execInfo.lpVerb = _T("runas");
  execInfo.cbSize = sizeof(execInfo);
  execInfo.nShow = SW_SHOWNORMAL;
  execInfo.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
  execInfo.hwnd = NULL;
  execInfo.hProcess = NULL;
  if (::ShellExecuteEx(&execInfo) && execInfo.hProcess != NULL) {
    ::WaitForSingleObject(execInfo.hProcess, INFINITE);
    DWORD dwExitCode = 0;
    ::GetExitCodeProcess(execInfo.hProcess, &dwExitCode);
    ::CloseHandle(execInfo.hProcess);
    return dwExitCode;
  }
  return -1;
}
