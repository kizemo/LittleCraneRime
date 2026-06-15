#include <windows.h>

#include <stdio.h>

#include <string>
#include <vector>

static int g_failures = 0;

static void Fail(const wchar_t* msg) {
  wprintf(L"FAIL: %s\n", msg);
  ++g_failures;
}

static void Pass(const wchar_t* msg) { wprintf(L"PASS: %s\n", msg); }

static bool ReadRegString(HKEY root,
                          const wchar_t* subkey,
                          const wchar_t* name,
                          REGSAM flags,
                          std::wstring& out) {
  HKEY hKey = NULL;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ | flags, &hKey) != ERROR_SUCCESS)
    return false;
  wchar_t buf[MAX_PATH * 4] = {};
  DWORD sz = sizeof(buf);
  DWORD type = 0;
  const LSTATUS ret =
      RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)buf, &sz);
  RegCloseKey(hKey);
  if (ret != ERROR_SUCCESS || type != REG_SZ)
    return false;
  out = buf;
  return !out.empty();
}

static std::wstring GetWeaselRoot() {
  std::wstring root;
  const DWORD views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY, 0};
  for (DWORD view : views) {
    if (ReadRegString(HKEY_LOCAL_MACHINE, L"Software\\Rime\\Weasel",
                      L"WeaselRoot", view, root))
      return root;
  }
  return L"";
}

static ULONGLONG FileSize(const std::wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA fad = {};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
    return 0;
  ULARGE_INTEGER size;
  size.LowPart = fad.nFileSizeLow;
  size.HighPart = fad.nFileSizeHigh;
  return size.QuadPart;
}

static bool FileVersionStartsWith(const std::wstring& path,
                                  const std::wstring& prefix,
                                  std::wstring& full_version) {
  DWORD handle = 0;
  const DWORD sz =
      GetFileVersionInfoSizeW(path.c_str(), &handle);
  if (!sz)
    return false;
  std::vector<BYTE> data(sz);
  if (!GetFileVersionInfoW(path.c_str(), 0, sz, data.data()))
    return false;
  VS_FIXEDFILEINFO* info = NULL;
  UINT len = 0;
  if (!VerQueryValueW(data.data(), L"\\", (LPVOID*)&info, &len) || !info)
    return false;
  wchar_t buf[64];
  swprintf_s(buf, L"%u.%u.%u.%u", HIWORD(info->dwFileVersionMS),
             LOWORD(info->dwFileVersionMS), HIWORD(info->dwFileVersionLS),
             LOWORD(info->dwFileVersionLS));
  full_version = buf;
  return full_version.rfind(prefix, 0) == 0;
}

static std::wstring TempRimeWeaselPath(const wchar_t* leaf) {
  wchar_t temp[MAX_PATH] = {};
  if (!GetTempPathW(_countof(temp), temp))
    return L"";
  std::wstring path = temp;
  if (!path.empty() && path.back() != L'\\')
    path += L'\\';
  path += L"rime.weasel\\";
  path += leaf;
  return path;
}

static std::wstring ReadTextFile(const std::wstring& path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return L"";
  const DWORD sz = GetFileSize(h, NULL);
  if (sz == INVALID_FILE_SIZE || sz == 0) {
    CloseHandle(h);
    return L"";
  }
  std::wstring text;
  text.resize(sz / sizeof(wchar_t) + 1);
  DWORD read = 0;
  if (!ReadFile(h, text.data(), sz, &read, NULL)) {
    CloseHandle(h);
    return L"";
  }
  CloseHandle(h);
  text.resize(read / sizeof(wchar_t));
  if (text.size() >= 1 && text[0] == 0xFEFF)
    text.erase(0, 1);
  return text;
}

static bool ContainsLine(const std::wstring& haystack,
                         const std::wstring& needle,
                         std::wstring& matched_line) {
  size_t pos = 0;
  while (pos < haystack.size()) {
    size_t end = haystack.find(L'\n', pos);
    if (end == std::wstring::npos)
      end = haystack.size();
    std::wstring line = haystack.substr(pos, end - pos);
    if (!line.empty() && line.back() == L'\r')
      line.pop_back();
    if (line.find(needle) != std::wstring::npos) {
      matched_line = line;
      return true;
    }
    pos = end + 1;
  }
  return false;
}

static void PrintInstallLogTail() {
  const std::wstring log = TempRimeWeaselPath(L"install.log");
  const std::wstring text = ReadTextFile(log);
  if (text.empty())
    return;
  std::vector<std::wstring> lines;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find(L'\n', pos);
    if (end == std::wstring::npos)
      end = text.size();
    std::wstring line = text.substr(pos, end - pos);
    if (!line.empty() && line.back() == L'\r')
      line.pop_back();
    lines.push_back(line);
    pos = end + 1;
  }
  const size_t start = lines.size() > 8 ? lines.size() - 8 : 0;
  for (size_t i = start; i < lines.size(); ++i)
    wprintf(L"install.log: %s\n", lines[i].c_str());
}

static bool IsArm64Machine() {
  USHORT processMachine = 0;
  USHORT nativeMachine = 0;
  using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
  const auto fn = reinterpret_cast<IsWow64Process2Fn>(
      GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process2"));
  if (fn && fn(GetCurrentProcess(), &processMachine, &nativeMachine))
    return nativeMachine == IMAGE_FILE_MACHINE_ARM64;
  SYSTEM_INFO si = {};
  GetNativeSystemInfo(&si);
  return si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
}

static std::wstring InstallSourceDllName() {
  if (IsArm64Machine())
    return L"weaselARM64X.dll";
  return L"weaselx64.dll";
}

static std::wstring ParseExpectedVersion(int argc, wchar_t** argv) {
  std::wstring expected = L"0.18.48";
  for (int i = 1; i < argc; ++i) {
    if (wcsncmp(argv[i], L"/version:", 9) == 0)
      expected = argv[i] + 9;
    else if (argv[i][0] != L'/' && argv[i][0] != L'-')
      expected = argv[i];
  }
  return expected;
}

int wmain(int argc, wchar_t** argv) {
  const std::wstring expected = ParseExpectedVersion(argc, argv);

  const std::wstring weasel_root = GetWeaselRoot();
  if (weasel_root.empty()) {
    Fail(L"HKLM\\Software\\Rime\\Weasel\\WeaselRoot is empty (checked 64-bit "
         L"and WOW6432Node views)");
  } else {
    std::wstring msg = L"WeaselRoot=" + weasel_root;
    Pass(msg.c_str());
  }

  wchar_t sysdir[MAX_PATH] = {};
  GetSystemDirectoryW(sysdir, _countof(sysdir));
  const std::wstring src =
      weasel_root.empty() ? L"" : weasel_root + L"\\" + InstallSourceDllName();
  const std::wstring sys = std::wstring(sysdir) + L"\\weasel.dll";

  if (weasel_root.empty() || GetFileAttributesW(src.c_str()) ==
                                  INVALID_FILE_ATTRIBUTES) {
    std::wstring msg = L"Missing " + src;
    Fail(msg.c_str());
  }
  if (GetFileAttributesW(sys.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wstring msg = L"Missing " + sys;
    Fail(msg.c_str());
  }

  if (GetFileAttributesW(src.c_str()) != INVALID_FILE_ATTRIBUTES &&
      GetFileAttributesW(sys.c_str()) != INVALID_FILE_ATTRIBUTES) {
    const ULONGLONG src_len = FileSize(src);
    const ULONGLONG sys_len = FileSize(sys);
    if (src_len != sys_len) {
      wchar_t buf[256];
      swprintf_s(buf, L"System32 size (%llu) != install size (%llu)", sys_len,
                 src_len);
      Fail(buf);
    } else {
      wchar_t buf[256];
      swprintf_s(buf, L"System32 weasel.dll size matches install dir (%llu bytes)",
                 sys_len);
      Pass(buf);
    }

    std::wstring sys_ver;
    if (!FileVersionStartsWith(sys, expected, sys_ver)) {
      wchar_t buf[256];
      swprintf_s(buf, L"System32 version=%s expected %s*",
                 sys_ver.empty() ? L"?" : sys_ver.c_str(), expected.c_str());
      Fail(buf);
    } else {
      wchar_t buf[256];
      swprintf_s(buf, L"System32 version=%s", sys_ver.c_str());
      Pass(buf);
    }
  }

  const std::wstring tsf_log = TempRimeWeaselPath(L"weasel.tsf.log");
  if (GetFileAttributesW(tsf_log.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wstring msg = L"Missing " + tsf_log;
    Fail(msg.c_str());
  } else {
    std::wstring text = ReadTextFile(tsf_log);
    if (text.empty()) {
      HANDLE h = CreateFileW(tsf_log.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (h != INVALID_HANDLE_VALUE) {
        const DWORD sz = GetFileSize(h, NULL);
        std::string narrow(sz + 1, '\0');
        DWORD read = 0;
        if (ReadFile(h, narrow.data(), sz, &read, NULL)) {
          narrow.resize(read);
          text.assign(narrow.begin(), narrow.end());
        }
        CloseHandle(h);
      }
    }

    std::wstring loaded_needle =
        L"weasel.dll loaded " + expected;
    std::wstring active_needle = L"WeaselTSF " + expected + L" ActivateEx";
    std::wstring loaded_line;
    std::wstring active_line;
    if (!ContainsLine(text, loaded_needle, loaded_line)) {
      std::wstring msg =
          L"weasel.tsf.log lacks weasel.dll loaded " + expected;
      Fail(msg.c_str());
    } else {
      Pass(loaded_line.c_str());
    }
    if (!ContainsLine(text, active_needle, active_line)) {
      std::wstring msg =
          L"weasel.tsf.log lacks WeaselTSF " + expected + L" ActivateEx";
      Fail(msg.c_str());
    } else {
      Pass(active_line.c_str());
    }
  }

  PrintInstallLogTail();

  if (g_failures > 0) {
    wprintf(L"\nVerification FAILED (%d issue(s)).\n", g_failures);
    return 1;
  }

  wprintf(L"\nVerification PASSED. Restart WeChat before input testing.\n");
  return 0;
}
