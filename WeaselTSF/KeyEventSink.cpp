#include "stdafx.h"

#include "WeaselIPC.h"

#include "WeaselTSF.h"

#include <KeyEvent.h>

#include <ShiftComboGuard.h>

#include <WeaselAsciiTrace.h>
#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>
#include <WeaselTSFCallbackSafe.h>

#include <resource.h>

#include "CandidateList.h"
#include "Globals.h"

static weasel::KeyEvent prevKeyEvent;
static BOOL prevfEaten = FALSE;
static int keyCountToSimulate = 0;

namespace {

bool IsShiftVKey(WPARAM vkey) {
  return vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT;
}

bool IsKeyUp(LPARAM lParam) {
  return (lParam & (1u << 31)) != 0;
}

void ResetAsciiComposerShiftLatch(weasel::Client& client) {
  (void)client;
}

int DeleteCandidateIndexFromKey(WPARAM wParam) {
  if (wParam >= L'1' && wParam <= L'9')
    return static_cast<int>(wParam - L'1');
  if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9)
    return static_cast<int>(wParam - VK_NUMPAD1);
  return -1;
}

bool IsLeftShiftKey(WPARAM wParam, LPARAM lParam) {
  if (wParam == VK_LSHIFT)
    return true;
  if (wParam == VK_RSHIFT)
    return false;
  if (wParam == VK_SHIFT)
    return ((lParam >> 16) & 0xFF) != 0x36;
  return false;
}

bool IsImeShiftComboKey(WPARAM wParam) {
  if (wParam >= 'A' && wParam <= 'Z')
    return false;
  if (wParam >= 'a' && wParam <= 'z')
    return false;
  if (wParam >= '0' && wParam <= '9')
    return false;
  if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
    return false;
  return true;
}

bool IsPhysicalShiftComboKey(WPARAM wParam) {
  if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)
    return false;
  if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL)
    return false;
  if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU)
    return false;
  if (wParam == VK_SPACE)
    return false;
  return IsImeShiftComboKey(wParam);
}

}  // namespace

bool WeaselTSF::_ShouldAllowShiftCandidatePick(WPARAM wParam,
                                               LPARAM lParam,
                                               bool key_up) const {
  if (!key_up || !_cand || _status.ascii_mode)
    return false;
  if (ShiftComboGuard::g_shift_combo || ShiftComboGuard::g_non_shift_key_seen)
    return false;

  UINT cand_count = 0;
  _cand->GetCount(&cand_count);
  const bool has_menu = cand_count >= 2;
  const bool composing = !!_status.composing;
  if (!has_menu && !composing)
    return false;

  if (IsLeftShiftKey(wParam, lParam))
    return has_menu || composing;
  if (wParam == VK_RSHIFT ||
      (wParam == VK_SHIFT && !IsLeftShiftKey(wParam, lParam)))
    return cand_count >= 3 || (composing && cand_count >= 2);
  return false;
}

bool WeaselTSF::_HasVisibleCandidateMenu() const {
  if (!_cand || _status.ascii_mode)
    return false;
  UINT cand_count = 0;
  if (_cand->GetCount(&cand_count) != S_OK)
    return false;
  return cand_count >= 2 || (_status.composing && cand_count >= 1);
}

void WeaselTSF::_ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  try {
    _ProcessKeyEventImpl(wParam, lParam, pfEaten);
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-event",
                     L"uncaught exception in _ProcessKeyEvent");
    *pfEaten = FALSE;
  }
}

void WeaselTSF::_KeySinkReset() {
  _key_sink.ResetAll();
}

void WeaselTSF::_ProcessKeyEventImpl(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  _rime_key_dispatched = FALSE;
  const bool key_up = IsKeyUp(lParam);
  const bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  const bool is_shift_vkey = IsShiftVKey(wParam);

  if (!key_up && wParam == VK_SPACE && ctrl_down && !shift_down && !alt_down) {
    if (_EnsureServerConnected()) {
      ShiftComboGuard::OnShiftKeyUpAlone();
      _ToggleAsciiMode();
      *pfEaten = TRUE;
    } else {
      *pfEaten = FALSE;
    }
    return;
  }

  if (!key_up && _CanDeleteCandidate()) {
    if (!_EnsureServerConnected()) {
      *pfEaten = FALSE;
      return;
    }
    if (wParam == VK_DELETE && ctrl_down && !shift_down && _cand) {
      const size_t index = _cand->GetHighlightedIndex();
      wchar_t buf[64];
      swprintf_s(buf, L"Ctrl+Delete index=%zu", index);
      WeaselAppendLogW(L"weasel.ui.log", L"delete", buf);
      _DeleteCandidateOnCurrentPage(index);
      *pfEaten = TRUE;
      return;
    }
    if (ctrl_down && shift_down) {
      const int key_index = DeleteCandidateIndexFromKey(wParam);
      if (key_index >= 0) {
        UINT cand_count = 0;
        _cand->GetCount(&cand_count);
        const size_t index = static_cast<size_t>(key_index);
        *pfEaten = TRUE;
        if (index < cand_count) {
          wchar_t buf[80];
          swprintf_s(buf, L"Shift+Ctrl+%d delete index=%zu", key_index + 1,
                     index);
          WeaselAppendLogW(L"weasel.ui.log", L"delete", buf);
          _DeleteCandidateOnCurrentPage(index);
        }
        return;
      }
    }
  }

  if ((_isToOpenClose && !_IsKeyboardOpen()) || _IsKeyboardDisabled()) {
    if (!is_shift_vkey || !_status.composing) {
      *pfEaten = FALSE;
      return;
    }
  }

  _ResetStaleCompositionState();

  if (!_EnsureServerConnected()) {
    *pfEaten = FALSE;
    return;
  }

  if (!m_client.EnsureSession())
    m_client.StartSession();

  const bool is_shift = IsShiftVKey(wParam);

  if (is_shift && _ShouldIsolateShiftForComposing()) {
    if (!key_up) {
      ShiftComboGuard::OnShiftKeyDown();
      if (!ShiftComboGuard::g_letter_key_seen) {
        *pfEaten = TRUE;
        return;
      }
    } else {
      const bool left = IsLeftShiftKey(wParam, lParam);
      const bool right =
          wParam == VK_RSHIFT || (wParam == VK_SHIFT && !left);
      if (!ShiftComboGuard::g_letter_key_seen) {
        if (left) {
          ShiftComboGuard::OnShiftKeyUpAlone();
          WeaselAppendLogW(L"weasel.tsf.log", L"candidate",
                           L"shift-pick index=1 composing=1");
          _SelectCandidateOnCurrentPage(1);
          *pfEaten = TRUE;
          return;
        }
        if (right) {
          ShiftComboGuard::OnShiftKeyUpAlone();
          WeaselAppendLogW(L"weasel.tsf.log", L"candidate",
                           L"shift-pick index=2 composing=1");
          _SelectCandidateOnCurrentPage(2);
          *pfEaten = TRUE;
          return;
        }
      }
      ShiftComboGuard::OnShiftKeyUpAfterCombo();
      WeaselAppendLogW(L"weasel.tsf.log", L"candidate",
                       L"shift-eat composing shift-combo");
      *pfEaten = TRUE;
      return;
    }
  }

  if (is_shift && !key_up)
    ShiftComboGuard::OnShiftKeyDown();

  if (is_shift && key_up) {
    if (!ShiftComboGuard::ShouldBlockShiftToggle()) {
      ShiftComboGuard::OnShiftKeyUpAlone();
      WeaselAppendLogW(L"weasel.tsf.log", L"toggle",
                       L"shift release -> _ToggleAsciiMode (lock_rime=1)");
      // 0.18.97: shift 单独释放触发的 toggle 需要锁定 RIME 引擎状态，
      // 防止后续 RIME 处理"反方向"切换（如 schema 定义的字母键切换）覆盖结果
      _ToggleAsciiMode(TRUE);
      *pfEaten = TRUE;
      return;
    }
    if (ShiftComboGuard::ShouldEatShiftRelease()) {
      WeaselAsciiTraceShiftRelease(
          true,
          (ShiftComboGuard::g_letter_key_seen ? 1 : 0) +
              (ShiftComboGuard::g_non_shift_key_seen ? 2 : 0),
          ShiftComboGuard::g_shift_combo);
      ShiftComboGuard::OnShiftKeyUpAfterCombo();
      *pfEaten = TRUE;
      return;
    }
  }

  if (!is_shift && !key_up)
    ShiftComboGuard::OnLetterOrDigitDuringShift(wParam);

  if (!is_shift && !key_up && shift_down) {
    if (IsPhysicalShiftComboKey(wParam))
      ShiftComboGuard::OnPhysicalShiftCombo();
  }

  if (!is_shift && !key_up) {
    if (ShiftComboGuard::g_shift_armed && IsImeShiftComboKey(wParam))
      ShiftComboGuard::OnComboKeyDown();
    if (shift_down && IsPhysicalShiftComboKey(wParam)) {
      ShiftComboGuard::OnPhysicalShiftCombo();
    }
  }

  weasel::KeyEvent ke;
  GetKeyboardState(_lpbKeyState);
  if (!ConvertKeyEvent(static_cast<UINT>(wParam), lParam, _lpbKeyState, ke)) {
    if (!is_shift && !key_up && ShiftComboGuard::g_shift_armed &&
        IsImeShiftComboKey(wParam))
      ShiftComboGuard::OnComboKeyDown();
    *pfEaten = FALSE;
  } else {
    if (_cand->GetIsReposition()) {
      if (ke.keycode == ibus::Up)
        ke.keycode = ibus::Down;
      else if (ke.keycode == ibus::Down)
        ke.keycode = ibus::Up;
    }

    if (!keyCountToSimulate) {
      if (ke.keycode == ibus::space && (ke.mask & ibus::CONTROL_MASK))
        _ascii_toggle_tick_ = 0;
      *pfEaten = (BOOL)m_client.ProcessKeyEvent(ke);
      _rime_key_dispatched = TRUE;
    }

    if (ke.keycode == ibus::Caps_Lock) {
      if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
          (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
        if ((GetKeyState(VK_CAPITAL) & 0x01)) {
          if (_committed || (!*pfEaten && _status.composing)) {
            keyCountToSimulate = 2;
            INPUT inputs[2];
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
            ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs, sizeof(INPUT));
          }
        }
        *pfEaten = TRUE;
      }
      if (keyCountToSimulate)
        keyCountToSimulate--;
    }

    if (is_shift && key_up)
      ShiftComboGuard::OnShiftKeyUpAlone();

    prevfEaten = *pfEaten;
    prevKeyEvent = ke;
  }
}

namespace {

bool IsWeaselOwnedForegroundWindow(HWND hwnd) {
  if (!hwnd)
    return false;
  wchar_t cls[64] = {};
  GetClassNameW(hwnd, cls, 64);
  if (wcsstr(cls, L"WeaselTray") != nullptr)
    return true;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId())
    return false;
  const LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
  return (ex & WS_EX_TOOLWINDOW) != 0;
}

}  // namespace

STDAPI WeaselTSF::OnSetFocus(BOOL fForeground) {
  return weasel_tsf_safe::RunTSFCallback(L"OnSetFocus", [&]() -> HRESULT {
  try {
    if (fForeground) {
      _KeySinkReset();
      _ResetStaleCompositionState();
      HWND focus_hwnd = _GetFocusedContextWindow();
      if (focus_hwnd)
        _last_focus_hwnd_ = focus_hwnd;
      m_client.FocusIn();
      weasel_crash_diag::Crumb(L"focus", L"SetFocus in");
    } else {
      HWND fg = GetForegroundWindow();
      if (!IsWeaselOwnedForegroundWindow(fg)) {
        m_client.FocusOut();
        weasel_crash_diag::Crumb(L"focus", L"SetFocus out");
      }
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"focus",
                     L"uncaught exception in OnSetFocus");
    weasel_crash_diag::Crumb(L"focus", L"OnSetFocus ex");
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnTestKeyDown(ITfContext* pContext,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL* pfEaten) {
  return weasel_tsf_safe::RunTSFCallback(L"OnTestKeyDown", [&]() -> HRESULT {
  try {
    _key_sink.ResetUp();
    if (_key_sink.down_pending) {
      *pfEaten = _key_sink.eaten;
      return S_OK;
    }
    _rime_key_dispatched = FALSE;
    _key_sink.ResetDown();
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    _key_sink.eaten = *pfEaten;
    _key_sink.down_seen = TRUE;
    _key_sink.down_rime = _rime_key_dispatched;
    if (*pfEaten)
      _key_sink.down_pending = TRUE;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-event",
                     L"uncaught exception in OnTestKeyDown");
    weasel_crash_diag::Crumb(L"key-event", L"OnTestKeyDown ex");
    *pfEaten = FALSE;
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnKeyDown(ITfContext* pContext,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL* pfEaten) {
  return weasel_tsf_safe::RunTSFCallback(L"OnKeyDown", [&]() -> HRESULT {
  try {
    _key_sink.ResetUp();
    _key_sink.down_after_rime = FALSE;

    BOOL after_rime = FALSE;
    if (_key_sink.down_pending) {
      _key_sink.down_pending = FALSE;
      *pfEaten = _key_sink.eaten;
      _key_sink.down_seen = FALSE;
      after_rime = _key_sink.down_rime;
    } else if (_key_sink.down_seen) {
      _key_sink.down_seen = FALSE;
      *pfEaten = _key_sink.eaten;
      after_rime = _key_sink.down_rime;
    } else {
      _ProcessKeyEvent(wParam, lParam, pfEaten);
      after_rime = _rime_key_dispatched;
    }

    if (after_rime) {
      _AfterRimeKey(pContext, *pfEaten);
      _key_sink.down_after_rime = TRUE;
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-event",
                     L"uncaught exception in OnKeyDown");
    weasel_crash_diag::Crumb(L"key-event", L"OnKeyDown ex");
    *pfEaten = FALSE;
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnTestKeyUp(ITfContext* pContext,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL* pfEaten) {
  return weasel_tsf_safe::RunTSFCallback(L"OnTestKeyUp", [&]() -> HRESULT {
  try {
    _key_sink.ResetDown();
    if (_key_sink.up_pending) {
      *pfEaten = TRUE;
      return S_OK;
    }
    _rime_key_dispatched = FALSE;
    _key_sink.ResetUp();
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    _key_sink.up_seen = TRUE;
    _key_sink.up_rime = _rime_key_dispatched;
    if (*pfEaten)
      _key_sink.up_pending = TRUE;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-event",
                     L"uncaught exception in OnTestKeyUp");
    weasel_crash_diag::Crumb(L"key-event", L"OnTestKeyUp ex");
    *pfEaten = FALSE;
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnKeyUp(ITfContext* pContext,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL* pfEaten) {
  return weasel_tsf_safe::RunTSFCallback(L"OnKeyUp", [&]() -> HRESULT {
  try {
    _key_sink.ResetDown();

    if (_key_sink.down_after_rime) {
      _key_sink.down_after_rime = FALSE;
      *pfEaten = FALSE;
      return S_OK;
    }

    if (_key_sink.up_pending) {
      _key_sink.up_pending = FALSE;
      *pfEaten = TRUE;
      _key_sink.up_seen = FALSE;
      if (_key_sink.up_rime)
        _AfterRimeKey(pContext, *pfEaten);
    } else if (_key_sink.up_seen) {
      _key_sink.up_seen = FALSE;
      *pfEaten = TRUE;
      if (_key_sink.up_rime)
        _AfterRimeKey(pContext, *pfEaten);
    } else {
      _ProcessKeyEvent(wParam, lParam, pfEaten);
      if (_rime_key_dispatched)
        _AfterRimeKey(pContext, *pfEaten);
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-event",
                     L"uncaught exception in OnKeyUp");
    weasel_crash_diag::Crumb(L"key-event", L"OnKeyUp ex");
    *pfEaten = FALSE;
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnPreservedKey(ITfContext* pContext,
                                 REFGUID rguid,
                                 BOOL* pfEaten) {
  try {
    *pfEaten = FALSE;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"preserved-key",
                     L"uncaught exception in OnPreservedKey");
    weasel_crash_diag::Crumb(L"preserved-key", L"OnPreservedKey ex");
    *pfEaten = FALSE;
  }
  return S_OK;
}

BOOL WeaselTSF::_InitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  HRESULT hr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this,
                                         TRUE);

  return (hr == S_OK);
}

void WeaselTSF::_UninitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
}

BOOL WeaselTSF::_InitPreservedKey() {
  com_ptr<ITfKeystrokeMgr> keystroke_mgr;
  if (_pThreadMgr->QueryInterface(&keystroke_mgr) == S_OK) {
    TF_PRESERVEDKEY key = {};
    key.uVKey = VK_SHIFT;
    key.uModifiers = TF_MOD_ON_KEYUP;
    keystroke_mgr->UnpreserveKey(GUID_IME_MODE_PRESERVED_KEY, &key);
  }
  return TRUE;
}

void WeaselTSF::_UninitPreservedKey() {
}
