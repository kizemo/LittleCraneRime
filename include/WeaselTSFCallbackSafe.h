#pragma once
// =============================================================================
// WeaselTSFCallbackSafe.h
// 0.18.96: 统一的 TSF 回调保护层
//
// 目的：所有 TSF 框架回调入口（OnKeyDown/OnTestKeyDown/OnSetFocus/ActivateEx 等）
//       都通过本模块提供的 SEH 翻译 + C++ 异常捕获双层保护。
//
// 之前 0.18.94 引入的 SehTranslatorScope 仅覆盖了 DoEditSession，导致其他
// TSF 回调入口的 C++ 异常（0xE06D7363）无法被 catch(...) 捕获而终止宿主进程
// （如 Weixin.exe / Cursor.exe / TRAE SOLO.exe）。
//
// 工作机制：
//   1. RAII 在调用栈顶端安装 _set_se_translator
//   2. SEH 异常（C++ EH 抛出的 0xE06D7363 或其他）被翻译成 seh_escape_exception
//   3. 翻译后的 C++ 异常被 catch(...) 捕获，写入 weasel.crash.log
//   4. 宿主进程继续运行而不是崩溃
// =============================================================================

#include <Windows.h>
#include <exception>
#include <type_traits>

#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>

namespace weasel_tsf_safe {

// -----------------------------------------------------------------------------
// SEH 异常对象（与 EditSession.cpp 中的定义保持一致）
// -----------------------------------------------------------------------------
class seh_escape_exception : public std::exception {
 public:
  explicit seh_escape_exception(unsigned int code, EXCEPTION_POINTERS* ep)
      : code_(code), ep_(ep) {}
  unsigned int code() const { return code_; }
  EXCEPTION_POINTERS* info() const { return ep_; }

 private:
  unsigned int code_;
  EXCEPTION_POINTERS* ep_;
};

// -----------------------------------------------------------------------------
// SEH 翻译 thunk：把 RaiseException(0xE06D7363, ...) 转成 C++ throw
// -----------------------------------------------------------------------------
inline void seh_translator_thunk(unsigned int code, EXCEPTION_POINTERS* ep) {
  throw seh_escape_exception(code, ep);
}

// -----------------------------------------------------------------------------
// RAII 翻译作用域：在作用域内把 SEH 异常翻译成 C++ 异常
// -----------------------------------------------------------------------------
class SehTranslatorScope {
 public:
  SehTranslatorScope() {
    prev_ = _set_se_translator(seh_translator_thunk);
  }
  ~SehTranslatorScope() {
    _set_se_translator(prev_);
  }

 private:
  _se_translator_function prev_;
};

// -----------------------------------------------------------------------------
// 把 seh_escape_exception 写日志
// -----------------------------------------------------------------------------
inline void LogSehEscape(const wchar_t* tag,
                         const seh_escape_exception& e) {
  EXCEPTION_POINTERS* ep = e.info();
  wchar_t buf[256];
  swprintf_s(buf, L"SEH escape in %s code=0x%08X addr=%p",
             tag, e.code(),
             (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress
                                        : nullptr);
  WeaselAppendLogW(L"weasel.crash.log", L"seh-escape", buf);
  weasel_crash_diag::Crumb(L"seh-escape", buf);
}

// -----------------------------------------------------------------------------
// RunTSFCallback：在 TSF 回调入口执行 lambda，捕获所有 C++ / SEH 异常
// 返回值通过引用参数传出，函数本身返回 bool 表示是否成功完成
//
// 用法：
//   STDAPI WeaselTSF::OnKeyDown(...) {
//     return weasel_tsf_safe::RunTSFCallback(L"OnKeyDown", [&]() -> HRESULT {
//       ...原始逻辑...
//       return S_OK;
//     });
//   }
// -----------------------------------------------------------------------------
template <typename Fn>
inline HRESULT RunTSFCallback(const wchar_t* tag, Fn&& fn) {
  SehTranslatorScope seh;
  try {
    return fn();
  } catch (const seh_escape_exception& e) {
    LogSehEscape(tag, e);
    weasel_crash_diag::Crumb(L"tsf-exc",
                             std::wstring(tag) + L" seh-escape caught");
    return E_FAIL;
  } catch (const std::exception& ex) {
    wchar_t buf[256];
    swprintf_s(buf, L"std::exception in %s: %hs", tag,
               typeid(ex).name());
    WeaselAppendLogW(L"weasel.crash.log", L"tsf-exc", buf);
    weasel_crash_diag::Crumb(L"tsf-exc", buf);
    return E_FAIL;
  } catch (...) {
    wchar_t buf[128];
    swprintf_s(buf, L"unknown exception in %s", tag);
    WeaselAppendLogW(L"weasel.crash.log", L"tsf-exc", buf);
    weasel_crash_diag::Crumb(L"tsf-exc", buf);
    return E_FAIL;
  }
}

}  // namespace weasel_tsf_safe