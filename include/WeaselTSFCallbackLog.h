#pragma once
// =============================================================================
// WeaselTSFCallbackLog.h
// 详细 TSF 框架回调日志（0.18.93）
//
// 目的：在 WeaselTSF.dll 注入到宿主进程时，记录 TSF 框架的关键回调，
//       帮助分析宿主进程（如 WeChat / Trae / Cursor）崩溃时的时序信息。
//
// 日志输出：
//   %TEMP%\rime.weasel\weasel.tsf_cb.log
//   每行格式：HH:MM:SS.mmm [tag] message
//
// 仅在 WEASEL_TSF_CB_LOG=1 环境变量开启时输出详细日志，避免性能影响。
// 启用方法：在系统环境变量或 WeaselServer 启动脚本中设置 WEASEL_TSF_CB_LOG=1
// =============================================================================
#include <Windows.h>
#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>
#include <string>

namespace weasel_tsf_cb_log {

// -----------------------------------------------------------------------------
// 检测是否启用详细 TSF 回调日志（默认关闭，需环境变量 WEASEL_TSF_CB_LOG=1）
// -----------------------------------------------------------------------------
inline bool Enabled() {
  static int cached = -1;
  if (cached < 0) {
    wchar_t buf[16] = {};
    DWORD len = GetEnvironmentVariableW(L"WEASEL_TSF_CB_LOG", buf, 16);
    if (len > 0 && len < 16) {
      cached = (_wtoi(buf) != 0) ? 1 : 0;
    } else {
      cached = 0;
    }
  }
  return cached == 1;
}

// -----------------------------------------------------------------------------
// 通用 TSF 回调入口日志
// tag: 回调名称（如 "OnSetFocus", "OnKeyDown", "ActivateEx"）
// detail: 附加信息（如 "session_id=12345 composing=1"）
// -----------------------------------------------------------------------------
inline void Enter(const wchar_t* tag, const std::wstring& detail = L"") {
  if (!Enabled()) return;
  weasel_crash_diag::Crumb(L"tsf-enter", std::wstring(tag) + L" " + detail);
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[32];
  swprintf_s(ts, L"%.2d:%.2d:%.2d.%.3d", st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
  std::wstring line = std::wstring(ts) + L" [>] " + tag + L" " + detail;
  WeaselAppendLogW(L"weasel.tsf_cb.log", L"enter", line);
}

inline void Leave(const wchar_t* tag, const std::wstring& detail = L"") {
  if (!Enabled()) return;
  weasel_crash_diag::Crumb(L"tsf-leave", std::wstring(tag) + L" " + detail);
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[32];
  swprintf_s(ts, L"%.2d:%.2d:%.2d.%.3d", st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
  std::wstring line = std::wstring(ts) + L" [<] " + tag + L" " + detail;
  WeaselAppendLogW(L"weasel.tsf_cb.log", L"leave", line);
}

// -----------------------------------------------------------------------------
// TSF 异常/异常路径日志（即使未启用详细日志也会写入）
// -----------------------------------------------------------------------------
inline void Exception(const wchar_t* tag,
                      const std::wstring& detail,
                      DWORD code = 0) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[32];
  swprintf_s(ts, L"%.2d:%.2d:%.2d.%.3d", st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
  wchar_t code_str[16] = {};
  if (code != 0)
    swprintf_s(code_str, L" code=0x%08X", code);
  std::wstring line = std::wstring(ts) + L" [!] " + tag + code_str + L" " +
                      detail;
  weasel_crash_diag::Crumb(L"tsf-exc", std::wstring(tag) + L" " + detail);
  WeaselAppendLogW(L"weasel.tsf_cb.log", L"exception", line);
  // 关键事件始终写入 weasel.crash.log
  WeaselAppendLogW(L"weasel.crash.log", L"tsf-exc", line);
}

// -----------------------------------------------------------------------------
// 状态变化日志（composition 状态、shift 状态等）
// -----------------------------------------------------------------------------
inline void State(const wchar_t* tag, const std::wstring& detail) {
  if (!Enabled()) return;
  weasel_crash_diag::Crumb(L"tsf-state", std::wstring(tag) + L" " + detail);
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t ts[32];
  swprintf_s(ts, L"%.2d:%.2d:%.2d.%.3d", st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
  std::wstring line = std::wstring(ts) + L" [S] " + tag + L" " + detail;
  WeaselAppendLogW(L"weasel.tsf_cb.log", L"state", line);
}

// -----------------------------------------------------------------------------
// 格式化辅助：把数字转为字符串
// -----------------------------------------------------------------------------
inline std::wstring FmtHex(DWORD v) {
  wchar_t buf[16];
  swprintf_s(buf, L"0x%08X", v);
  return buf;
}

inline std::wstring FmtDec(DWORD v) {
  wchar_t buf[16];
  swprintf_s(buf, L"%lu", v);
  return buf;
}

// -----------------------------------------------------------------------------
// RAII 风格的回调作用域日志（Enter on construction, Leave on destruction）
// -----------------------------------------------------------------------------
class Scope {
 public:
  Scope(const wchar_t* tag, const std::wstring& detail = L"")
      : tag_(tag), entered_(Enabled()) {
    if (entered_) Enter(tag, detail);
  }
  ~Scope() {
    if (entered_) Leave(tag_);
  }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;

 private:
  const wchar_t* tag_;
  bool entered_;
};

// -----------------------------------------------------------------------------
// Host 进程信息一次性快照（weasel.dll 加载时写入）
// -----------------------------------------------------------------------------
inline void WriteHostSnapshot(const wchar_t* reason) {
  wchar_t host[MAX_PATH] = {};
  GetModuleFileNameW(NULL, host, MAX_PATH);
  const wchar_t* base = wcsrchr(host, L'\\');
  base = base ? base + 1 : host;

  wchar_t ver[64];
  // 由调用方在宏中提供 WEASEL_VERSION；这里读取 weasel.props
  swprintf_s(ver, L"0.18.93");

  wchar_t msg[512];
  swprintf_s(msg,
             L"reason=%s host=%s ver=%s pid=%lu tid=%lu",
             reason, base, ver,
             GetCurrentProcessId(), GetCurrentThreadId());
  WeaselAppendLogW(L"weasel.tsf_cb.log", L"snapshot", msg);
  WeaselAppendLogW(L"weasel.crash.log", L"tsf-snapshot", msg);
}

}  // namespace weasel_tsf_cb_log