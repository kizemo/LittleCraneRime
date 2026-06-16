#pragma once

#include <windows.h>

#include <shellapi.h>

#include <tlhelp32.h>

#include <WeaselFileLog.h>

#include <cstdio>

#pragma comment(lib, "shell32.lib")

namespace weasel_tray {

constexpr UINT kWeaselTrayNotifyId = 0x5E50;

inline const GUID& WeaselTrayNotifyGuid() {
  static const GUID guid = {0xa8f3e2d1,
                            0x6c4b,
                            0x4f9e,
                            {0x9d, 0x2a, 0x1e, 0x5f, 0x8b, 0x7c, 0x3d, 0x04}};
  return guid;
}

inline void SetTrayAppIdentity() {
  SetCurrentProcessExplicitAppUserModelID(L"Rime.Weasel.Input.Tray");
}

inline void KillOtherWeaselServerProcesses() {
  DWORD self = GetCurrentProcessId();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;

  PROCESSENTRY32W pe = {sizeof(pe)};
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"WeaselServer.exe") == 0 &&
          pe.th32ProcessID != self) {
        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
        if (proc) {
          TerminateProcess(proc, 0);
          CloseHandle(proc);
        }
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
}

inline void CleanupDuplicateRunEntries() {
  wchar_t hkcu_path[MAX_PATH] = {};
  DWORD size = sizeof(hkcu_path);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                   L"WeaselServer", RRF_RT_REG_SZ, NULL, hkcu_path, &size) ==
      ERROR_SUCCESS) {
    wchar_t hklm_path[MAX_PATH] = {};
    size = sizeof(hklm_path);
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                     L"WeaselServer", RRF_RT_REG_SZ, NULL, hklm_path,
                     &size) == ERROR_SUCCESS) {
      RegDeleteKeyValueW(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                         L"WeaselServer");
    }
  }
}

inline void TrayLog(const wchar_t* action, UINT id, HWND hwnd, BOOL ok) {
  wchar_t buf[160];
  swprintf_s(buf, L"%s id=%u hwnd=%p ok=%d", action, id, hwnd, ok ? 1 : 0);
  WeaselAppendLogW(L"weasel.tray.log", L"tray", buf);
}

inline void DeleteTrayIconForWindow(HWND hwnd, UINT id) {
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = id;
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

inline void DeleteTrayIconByGuid(HWND hwnd) {
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(NOTIFYICONDATAW);
  nid.hWnd = hwnd;
  nid.uFlags = NIF_GUID;
  nid.guidItem = WeaselTrayNotifyGuid();
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

inline void PurgeWeaselTrayIconByGuid() {
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(NOTIFYICONDATAW);
  nid.uFlags = NIF_GUID;
  nid.guidItem = WeaselTrayNotifyGuid();
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

inline void CleanupKnownTrayIconIds(HWND hwnd) {
  if (!hwnd)
    return;
  const UINT ids[] = {105, 100, kWeaselTrayNotifyId, 1, 2, 3};
  for (UINT id : ids)
    DeleteTrayIconForWindow(hwnd, id);
  DeleteTrayIconByGuid(hwnd);
}

struct EnumTrayPurgeCtx {
  DWORD pid;
};

inline BOOL CALLBACK PurgeTrayIconsEnumProc(HWND hwnd, LPARAM lParam) {
  auto* ctx = reinterpret_cast<EnumTrayPurgeCtx*>(lParam);
  DWORD wpid = 0;
  GetWindowThreadProcessId(hwnd, &wpid);
  if (wpid != ctx->pid)
    return TRUE;
  CleanupKnownTrayIconIds(hwnd);
  return TRUE;
}

inline void PurgeProcessTrayIcons() {
  EnumTrayPurgeCtx ctx = {GetCurrentProcessId()};
  EnumWindows(PurgeTrayIconsEnumProc, reinterpret_cast<LPARAM>(&ctx));
}

inline void PurgeLegacyTrayIconIds() {
  const UINT legacy_ids[] = {0,  1,  2,  3,  99,  100, 101, 102, 103, 104,
                             105, kWeaselTrayNotifyId, 0x5E51, 0x5E52};
  for (UINT id : legacy_ids) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.uID = id;
    Shell_NotifyIconW(NIM_DELETE, &nid);
  }
}

inline bool NotifyIconEntryMatchesWeasel(HKEY item) {
  wchar_t path[MAX_PATH] = {};
  DWORD size = sizeof(path);
  if (RegGetValueW(item, NULL, L"ExecutablePath", RRF_RT_REG_SZ, NULL, path,
                   &size) == ERROR_SUCCESS) {
    if (wcsstr(path, L"WeaselServer.exe") || wcsstr(path, L"Rime\\weasel") ||
        wcsstr(path, L"rime\\weasel"))
      return true;
  }
  wchar_t tip[256] = {};
  size = sizeof(tip);
  if (RegGetValueW(item, NULL, L"InitialTooltip", RRF_RT_REG_SZ, NULL, tip,
                   &size) == ERROR_SUCCESS) {
    if (wcsstr(tip, L"小鹤") || wcsstr(tip, L"Weasel") || wcsstr(tip, L"Rime"))
      return true;
  }
  return false;
}

inline int PurgeNotifyIconSettingsAt(const wchar_t* base) {
  HKEY settings = NULL;
  const REGSAM access = KEY_READ | KEY_WRITE | KEY_ENUMERATE_SUB_KEYS;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, base, 0, access, &settings) !=
      ERROR_SUCCESS) {
    wchar_t buf[128];
    swprintf_s(buf, L"PurgeNotifyIconSettings open failed base=%s", base);
    WeaselAppendLogW(L"weasel.tray.log", L"tray", buf);
    return 0;
  }
  int removed = 0;
  int matched = 0;
  DWORD index = 0;
  wchar_t subkey[64];
  DWORD sublen = _countof(subkey);
  while (RegEnumKeyExW(settings, index, subkey, &sublen, NULL, NULL, NULL,
                       NULL) == ERROR_SUCCESS) {
    HKEY item = NULL;
    bool should_remove = false;
    if (RegOpenKeyExW(settings, subkey, 0, KEY_READ, &item) == ERROR_SUCCESS) {
      should_remove = NotifyIconEntryMatchesWeasel(item);
      RegCloseKey(item);
    }
    if (should_remove) {
      matched++;
      LSTATUS del = RegDeleteTreeW(settings, subkey);
      if (del == ERROR_SUCCESS) {
        removed++;
        sublen = _countof(subkey);
        continue;
      }
      wchar_t buf[128];
      swprintf_s(buf, L"PurgeNotifyIconSettings delete failed base=%s sub=%s err=%ld",
                 base, subkey, del);
      WeaselAppendLogW(L"weasel.tray.log", L"tray", buf);
    }
    index++;
    sublen = _countof(subkey);
  }
  RegCloseKey(settings);
  wchar_t buf[128];
  swprintf_s(buf, L"PurgeNotifyIconSettings base=%s matched=%d removed=%d", base,
             matched, removed);
  WeaselAppendLogW(L"weasel.tray.log", L"tray", buf);
  return removed;
}

inline int PurgeNotifyIconSettingsRegistry() {
  int removed = 0;
  removed += PurgeNotifyIconSettingsAt(L"Control Panel\\NotifyIconSettings");
  removed += PurgeNotifyIconSettingsAt(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
      L"NotifyIconSettings");
  if (removed > 0) {
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)L"TraySettings", SMTO_ABORTIFHUNG, 2000,
                        &result);
  }
  return removed;
}

inline void PurgeLightTrayIcons() {
  PurgeWeaselTrayIconByGuid();
  PurgeProcessTrayIcons();
}

inline void PurgeAllLegacyWeaselTrayIcons() {
  PurgeLightTrayIcons();
  PurgeLegacyTrayIconIds();
  PurgeNotifyIconSettingsRegistry();
  PurgeWeaselTrayIconByGuid();
}

}  // namespace weasel_tray
