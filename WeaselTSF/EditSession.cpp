#include "stdafx.h"

#include "WeaselTSF.h"

#include "CandidateList.h"

#include "ResponseParser.h"

#include <WeaselFileLog.h>

#include <WeaselCrashDiag.h>



namespace {

class EditSessionDepthGuard {
 public:
  explicit EditSessionDepthGuard(WeaselTSF* tsf) : tsf_(tsf) {
    tsf_->_EnterEditSession();
  }
  ~EditSessionDepthGuard() { tsf_->_LeaveEditSession(); }
 private:
  WeaselTSF* tsf_;
};

// 0.18.94: SEH/C++ exception unified translator
// Use _set_se_translator to convert uncaught SEH exceptions (0xE06D7363)
// to C++ exceptions that can be caught by catch(...).
// This works without requiring __try/__except (which conflicts with C++ EH).
class seh_escape_exception : public std::exception {
 public:
  explicit seh_escape_exception(unsigned int code,
                                EXCEPTION_POINTERS* ep)
      : code_(code), ep_(ep) {}
  unsigned int code() const { return code_; }
  EXCEPTION_POINTERS* info() const { return ep_; }
 private:
  unsigned int code_;
  EXCEPTION_POINTERS* ep_;
};

inline void seh_translator_thunk(unsigned int code,
                                 EXCEPTION_POINTERS* ep) {
  throw seh_escape_exception(code, ep);
}

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

inline void LogSehEscape(const wchar_t* scope,
                         const seh_escape_exception& e) {
  EXCEPTION_POINTERS* ep = e.info();
  wchar_t buf[256];
  swprintf_s(buf, L"SEH escape in %s code=0x%08X addr=%p",
             scope, e.code(),
             (ep && ep->ExceptionRecord)
                 ? ep->ExceptionRecord->ExceptionAddress
                 : nullptr);
  WeaselAppendLogW(L"weasel.crash.log", L"seh-escape", buf);
  weasel_crash_diag::Crumb(L"seh-escape", buf);
}

struct DoEditSessionArgs {
  WeaselTSF* pThis;
  TfEditCookie ec;
};

HRESULT DoEditSessionThunk(void* user) {
  DoEditSessionArgs* args = static_cast<DoEditSessionArgs*>(user);
  return args->pThis->DoEditSessionCoreImpl(args->ec);
}

}  // namespace

HRESULT __declspec(noinline) WeaselTSF::DoEditSessionCoreImpl(TfEditCookie ec) {
  try {
    EditSessionDepthGuard guard(this);

    const BOOL urgent = _ipc_refresh_urgent;
    const BOOL force = _ipc_refresh_force;
    _ipc_refresh_urgent = FALSE;
    _ipc_refresh_force = FALSE;

    const BOOL need_ipc = _ipc_refresh_requested || _composition_dirty;
    _ipc_refresh_requested = FALSE;
    _composition_dirty = FALSE;
    _edit_session_pending = FALSE;
    if (!need_ipc && !urgent) {
      weasel_crash_diag::Crumb(L"edit-session", L"skipped noop");
      return S_OK;
    }

    weasel_crash_diag::Crumb(L"edit-session", L"begin ipc");

    _FlushPendingTrayCommand();

    std::wstring commit;
    weasel::Config config;
    auto context = std::make_shared<weasel::Context>();
    bool ok = false;

    if (_key_pull.valid) {
      ok = _key_pull.ok != FALSE;
      commit = _key_pull.commit;
      if (_key_pull.context)
        context = _key_pull.context;
      config = _key_pull.config;
      if (_key_pull.style.font_point > 0)
        _cand->UpdateStyle(_key_pull.style);
      _key_pull.valid = FALSE;
    } else {
      weasel::ResponseParser parser(&commit, context.get(), &_status,
                                    &config, &_cand->style());
      try {
        ok = m_client.GetResponseData(std::ref(parser));
      } catch (...) {
        WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                         L"GetResponseData exception");
        weasel_crash_diag::Crumb(L"edit-session", L"ipc exception");
        return S_OK;
      }
    }

    wchar_t diag[128];
    swprintf_s(diag, L"ok=%d composing=%d preedit=%zu candies=%zu",
               ok ? 1 : 0, _status.composing ? 1 : 0,
               context->preedit.str.size(), context->cinfo.candies.size());
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session", diag);
    weasel_crash_diag::SetSnapshot(
        _edit_session_depth, _status.composing ? TRUE : FALSE,
        _server_connected ? TRUE : FALSE,
        static_cast<DWORD>(m_client.SessionId()));

    // 0.18.97: 恢复 force-sync，但仅在 _ascii_toggle_lock_rime_=true 时启用。
    // 之前 0.18.96 移除该逻辑后导致 shift 单独释放的 toggle 被 RIME 引擎的
    // "反方向"切换覆盖，状态在 1.5s 内被反转。
    // 现在仅在 shift 单独释放触发的 toggle（_ToggleAsciiMode(TRUE)）时锁定 RIME ；
    // shift+ctrl+其他按键 等组合场景下不会进入 _ToggleAsciiMode，因此 lock_rime=false，
    // force-sync 不生效，RIME 正常处理按键。
    //
    // 0.18.99.1: BUG 修复 - 不再在第一次确认匹配时立即清空 lock。
    // 见 Composition.cpp 中 _AfterRimeKey 的详细注释。
    BOOL langbar_force = FALSE;
    const DWORD now_tick = GetTickCount();
    const bool within_lock_window =
        _ascii_toggle_tick_ && (now_tick - _ascii_toggle_tick_) < 1500;
    if (_ascii_toggle_lock_rime_ &&
        _ascii_toggle_tick_ &&
        !!_status.ascii_mode != !!_ascii_toggle_value_) {
      // RIME 状态与用户选择的 toggle 不一致 → 强制同步
      _status.ascii_mode = !!_ascii_toggle_value_;
      _status.full_shape = !!_ascii_toggle_value_;
      langbar_force = TRUE;
    } else if (_ascii_toggle_tick_ && !within_lock_window &&
               !!_status.ascii_mode == !!_ascii_toggle_value_) {
      // 锁窗口外，且模式已稳定为目标值 → 释放 lock
      _ascii_toggle_tick_ = 0;
      _ascii_toggle_lock_rime_ = FALSE;
    }
    // 锁窗口内的确认匹配：保留 lock，不重置 tick（防止 1.5s 内下一次按键反弹）

    _UpdateLanguageBar(_status, !!langbar_force);

    if (ok) {
      if (!commit.empty()) {
        if (!_IsComposing()) {
          _StartComposition(_pEditSessionContext,
                            _fCUASWorkaroundEnabled && !config.inline_preedit);
        }
        _InsertText(_pEditSessionContext, commit);
        _EndCompositionInSession(ec, _pEditSessionContext, false);
        _committed = TRUE;
      } else {
        _committed = FALSE;
      }

      if (_status.composing && !_IsComposing()) {
        _StartComposition(_pEditSessionContext,
                          _fCUASWorkaroundEnabled && !config.inline_preedit);
      } else if (!_status.composing && _IsComposing()) {
        _EndUI();
        _EndCompositionInSession(ec, _pEditSessionContext, true);
      }

      if (_IsComposing() && config.inline_preedit) {
        _ShowInlinePreedit(_pEditSessionContext, context);
      }

      _deferred.layout_update = TRUE;
    }

    if (ok && _status.composing) {
      wchar_t cbuf[96];
      swprintf_s(cbuf, L"preedit=%zu candies=%zu",
                 context->preedit.str.size(), context->cinfo.candies.size());
      WeaselAppendLogW(L"weasel.tsf.log", L"compose", cbuf);
      _cand->StartUI();
    }

    try {
      _UpdateUI(*context, _status);
    } catch (...) {
      WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                       L"exception in _UpdateUI");
    }

    weasel_crash_diag::Crumb(L"edit-session", L"end ok");
    return S_OK;
  } catch (const std::exception& ex) {
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     L"C++ exception in DoEditSession");
    (void)ex;
    return E_FAIL;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     L"unknown exception in DoEditSession");
    return E_FAIL;
  }
}

STDAPI WeaselTSF::DoEditSession(TfEditCookie ec) {
  if (_edit_session_depth > 0) {
    _composition_dirty = TRUE;
    weasel_crash_diag::Crumb(L"edit-session", L"skip reentrant");
    return S_OK;
  }

  // 0.18.94: SEH/C++ exception translator scope
  // Catches C++ exceptions (0xE06D7363) that escape catch(...)
  // by converting them to C++ seh_escape_exception
  SehTranslatorScope sehScope;
  try {
    DoEditSessionArgs args;
    args.pThis = this;
    args.ec = ec;
    return DoEditSessionThunk(&args);
  } catch (const seh_escape_exception& e) {
    LogSehEscape(L"DoEditSession", e);
    return E_FAIL;
  } catch (const std::exception& ex) {
    (void)ex;
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     L"C++ std::exception in DoEditSession outer");
    return E_FAIL;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     L"unknown exception in DoEditSession outer");
    return E_FAIL;
  }
}