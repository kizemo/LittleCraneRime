#pragma once
#include <Windows.h>
#include <WeaselFileLog.h>
#include <array>
#include <mutex>
#include <string>

namespace weasel_crash_diag {

struct Snapshot {
  DWORD edit_session_depth = 0;
  BOOL composing = FALSE;
  BOOL ipc_connected = FALSE;
  DWORD session_id = 0;
  DWORD thread_id = 0;
};

constexpr size_t kMaxCrumbs = 40;

inline std::array<std::wstring, kMaxCrumbs>& CrumbRing() {
  static std::array<std::wstring, kMaxCrumbs> ring;
  return ring;
}

inline size_t& CrumbCount() {
  static size_t count = 0;
  return count;
}

inline std::mutex& CrumbMutex() {
  static std::mutex mu;
  return mu;
}

inline Snapshot& LastSnapshot() {
  static Snapshot snap;
  return snap;
}

inline void SetSnapshot(DWORD depth,
                        BOOL composing,
                        BOOL ipc_connected,
                        DWORD session_id) {
  auto& s = LastSnapshot();
  s.edit_session_depth = depth;
  s.composing = composing;
  s.ipc_connected = ipc_connected;
  s.session_id = session_id;
  s.thread_id = GetCurrentThreadId();
}

inline void Crumb(const wchar_t* tag, const std::wstring& message = L"") {
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[32];
  swprintf_s(ts, L"%.2d:%.2d:%.2d", st.wHour, st.wMinute, st.wSecond);
  std::wstring line = std::wstring(ts) + L" [" + tag + L"] " + message;
  std::lock_guard<std::mutex> lock(CrumbMutex());
  CrumbRing()[CrumbCount() % kMaxCrumbs] = line;
  ++CrumbCount();
}

inline void WriteCrashReport(EXCEPTION_POINTERS* ep) {
  wchar_t host[MAX_PATH] = {};
  GetModuleFileNameW(NULL, host, MAX_PATH);
  const wchar_t* host_base = wcsrchr(host, L'\\');
  host_base = host_base ? host_base + 1 : host;

  const DWORD code =
      (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
  const void* addr =
      (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress
                                  : nullptr;

  const auto& s = LastSnapshot();
  wchar_t summary[480];
  swprintf_s(summary,
             L"host=%s ver=" WEASEL_VERSION L" tid=%lu code=0x%08X addr=%p "
             L"depth=%lu composing=%d ipc=%d sess=%lu",
             host_base, s.thread_id, code, addr, s.edit_session_depth,
             s.composing ? 1 : 0, s.ipc_connected ? 1 : 0, s.session_id);

  WeaselAppendLogW(L"weasel.crash.log", L"report", summary);
  WeaselAppendLogW(L"weasel.tsf.log", L"crash-detail", summary);

  std::lock_guard<std::mutex> lock(CrumbMutex());
  const size_t total = CrumbCount();
  const size_t start = total > kMaxCrumbs ? total - kMaxCrumbs : 0;
  for (size_t i = start; i < total; ++i) {
    WeaselAppendLogW(L"weasel.crash.log", L"crumb",
                     CrumbRing()[i % kMaxCrumbs]);
  }
}

// 0.18.99.1: 节流 VEH crash 日志，避免每秒触发导致 weasel.crash.log 暴涨
// C++ 异常 (0xE06D7363) 在每个 TSF 回调里都被 catch 处理过，
// 因此高频记录它们不仅产生 GB 级日志，还消耗大量 CPU/磁盘 I/O 进而造成输入卡顿。
// 改用进程级 + 线程级双层节流：
//   * 进程级：每 (kCrashLogIntervalMs) 毫秒最多记录 1 次
//   * 线程级：每个线程 1 秒内最多 1 次
inline constexpr DWORD kCrashLogIntervalMs = 1000;

inline DWORD& LastGlobalLogTick() {
  static DWORD t = 0;
  return t;
}

struct ThreadLogThrottle {
  DWORD last_tick = 0;
};

inline ThreadLogThrottle& ThreadThrottle() {
  static thread_local ThreadLogThrottle t;
  return t;
}

inline LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* ep) {
  if (!ep || !ep->ExceptionRecord)
    return EXCEPTION_CONTINUE_SEARCH;
  const DWORD code = ep->ExceptionRecord->ExceptionCode;

  // 仅记录真正的崩溃码；0xE06D7363 (C++ 异常) 由 SEH translator 转为
  // seh_escape_exception 后被外层 catch(...) 捕获，不会让进程退出。
  if (code == 0x80000003 /* breakpoint */) {
    WriteCrashReport(ep);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (code != 0xE06D7363) {
    // 其它未知异常（访问违例等）也记录
    WriteCrashReport(ep);
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // 对 C++ 异常使用节流，防止日志风暴 + 卡顿
  const DWORD now = GetTickCount();
  ThreadLogThrottle& th = ThreadThrottle();
  if (now - th.last_tick < kCrashLogIntervalMs) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  th.last_tick = now;

  DWORD& global = LastGlobalLogTick();
  if (now - global < kCrashLogIntervalMs / 2) {
    // 全局层面也限速（不同线程可能并发触发）
    return EXCEPTION_CONTINUE_SEARCH;
  }
  global = now;

  WriteCrashReport(ep);
  return EXCEPTION_CONTINUE_SEARCH;
}

inline void Install() {
  static bool once = false;
  if (once)
    return;
  once = true;
  AddVectoredExceptionHandler(1, VectoredHandler);
}

}  // namespace
