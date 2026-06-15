#pragma once
#include <Windows.h>
#include <mutex>
#include <string>

inline std::wstring WeaselLogDirW() {
  WCHAR path[MAX_PATH] = {0};
  ExpandEnvironmentStringsW(L"%TEMP%\\rime.weasel", path, MAX_PATH);
  CreateDirectoryW(path, NULL);
  return path;
}

inline void WeaselAppendLogW(const wchar_t* filename,
                             const wchar_t* tag,
                             const std::wstring& message) {
  static std::mutex mu;
  std::lock_guard<std::mutex> lock(mu);
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[64];
  swprintf_s(ts, L"%.4d-%.2d-%.2d %.2d:%.2d:%.2d", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond);
  std::wstring path = WeaselLogDirW() + L"\\" + filename;
  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  std::wstring line =
      std::wstring(ts) + L" [" + tag + L"] " + message + L"\r\n";
  DWORD written = 0;
  WriteFile(h, line.c_str(),
            static_cast<DWORD>(line.size() * sizeof(wchar_t)), &written, NULL);
  FlushFileBuffers(h);
  CloseHandle(h);
}

inline void WeaselAppendLogA(const wchar_t* filename,
                             const char* tag,
                             const std::string& message) {
  int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
  if (len <= 0)
    return;
  std::wstring wmsg(len - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wmsg[0], len);
  std::wstring wtag = L"";
  if (tag) {
    int tlen = MultiByteToWideChar(CP_UTF8, 0, tag, -1, NULL, 0);
    if (tlen > 0) {
      wtag.resize(tlen - 1);
      MultiByteToWideChar(CP_UTF8, 0, tag, -1, &wtag[0], tlen);
    }
  }
  WeaselAppendLogW(filename, wtag.c_str(), wmsg);
}
