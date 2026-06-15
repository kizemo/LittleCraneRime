#include "stdafx.h"
#include "KeyCaptureEdit.h"

KeyCaptureEdit::KeyCaptureEdit() {}

void KeyCaptureEdit::SetKey(const std::wstring& key) {
  captured_ = key;
  if (m_hWnd)
    SetWindowTextW(captured_.empty() ? L"" : captured_.c_str());
}

std::wstring KeyCaptureEdit::GetKey() const {
  return captured_;
}

void KeyCaptureEdit::SetOnChange(std::function<void()> fn) {
  on_change_ = std::move(fn);
}

std::wstring KeyCaptureEdit::FormatKey(UINT vk, UINT flags) const {
  std::wstring chord;
  if (flags & MOD_CONTROL)
    chord += L"Control+";
  if (flags & MOD_ALT)
    chord += L"Alt+";
  if (flags & MOD_SHIFT)
    chord += L"Shift+";
  if (flags & MOD_WIN)
    chord += L"Super+";

  switch (vk) {
    case VK_RETURN:
      chord += L"Return";
      break;
    case VK_TAB:
      chord += L"Tab";
      break;
    case VK_SPACE:
      chord += L"space";
      break;
    case VK_BACK:
      chord += L"BackSpace";
      break;
    case VK_ESCAPE:
      chord += L"Escape";
      break;
    case VK_DELETE:
      chord += L"Delete";
      break;
    case VK_LEFT:
      chord += L"Left";
      break;
    case VK_RIGHT:
      chord += L"Right";
      break;
    case VK_UP:
      chord += L"Up";
      break;
    case VK_DOWN:
      chord += L"Down";
      break;
    case VK_PRIOR:
      chord += L"Page_Up";
      break;
    case VK_NEXT:
      chord += L"Page_Down";
      break;
    case VK_HOME:
      chord += L"Home";
      break;
    case VK_END:
      chord += L"End";
      break;
    case VK_OEM_COMMA:
      chord += L"comma";
      break;
    case VK_OEM_PERIOD:
      chord += L"period";
      break;
    case VK_OEM_MINUS:
      chord += L"minus";
      break;
    case VK_OEM_PLUS:
      chord += L"equal";
      break;
    case VK_OEM_4:
      chord += L"bracketleft";
      break;
    case VK_OEM_6:
      chord += L"bracketright";
      break;
    case VK_CAPITAL:
      chord += L"Caps_Lock";
      break;
    case VK_SHIFT:
    case VK_LSHIFT:
      chord += L"Shift_L";
      break;
    case VK_RSHIFT:
      chord += L"Shift_R";
      break;
    case VK_CONTROL:
    case VK_LCONTROL:
      chord += L"Control_L";
      break;
    case VK_RCONTROL:
      chord += L"Control_R";
      break;
    case VK_MENU:
    case VK_LMENU:
      chord += L"Alt_L";
      break;
    case VK_RMENU:
      chord += L"Alt_R";
      break;
    case VK_LWIN:
    case VK_RWIN:
      chord += L"Super";
      break;
    default:
      if (vk >= VK_F1 && vk <= VK_F24) {
        wchar_t buf[8];
        swprintf_s(buf, L"F%d", vk - VK_F1 + 1);
        chord += buf;
      } else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        chord += static_cast<wchar_t>(vk);
      } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        wchar_t buf[8];
        swprintf_s(buf, L"KP_%d", vk - VK_NUMPAD0);
        chord += buf;
      } else {
        return L"";
      }
      break;
  }
  return chord;
}

LRESULT KeyCaptureEdit::OnSetFocusMsg(UINT, WPARAM, LPARAM, BOOL&) {
  SetWindowTextW(L"按下快捷键...");
  SetSel(0, -1);
  return 0;
}

LRESULT KeyCaptureEdit::OnKillFocusMsg(UINT, WPARAM, LPARAM, BOOL&) {
  if (captured_.empty())
    SetWindowTextW(L"");
  else
    SetWindowTextW(captured_.c_str());
  return 0;
}

LRESULT KeyCaptureEdit::OnKeyDownMsg(UINT, WPARAM wParam, LPARAM, BOOL&) {
  UINT nChar = static_cast<UINT>(wParam);
  UINT mod = 0;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    mod |= MOD_CONTROL;
  if (GetKeyState(VK_MENU) & 0x8000)
    mod |= MOD_ALT;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    mod |= MOD_SHIFT;
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
    mod |= MOD_WIN;

  bool is_modifier =
      (nChar == VK_SHIFT || nChar == VK_LSHIFT || nChar == VK_RSHIFT ||
       nChar == VK_CONTROL || nChar == VK_LCONTROL || nChar == VK_RCONTROL ||
       nChar == VK_MENU || nChar == VK_LMENU || nChar == VK_RMENU ||
       nChar == VK_LWIN || nChar == VK_RWIN);
  if (is_modifier)
    return 0;

  if (nChar == VK_ESCAPE) {
    captured_.clear();
    SetWindowTextW(L"");
    if (on_change_)
      on_change_();
    return 0;
  }

  std::wstring chord = FormatKey(nChar, mod);
  if (chord.empty())
    return 0;
  captured_ = chord;
  SetWindowTextW(captured_.c_str());
  if (on_change_)
    on_change_();
  return 0;
}

LRESULT KeyCaptureEdit::OnChar(UINT, WPARAM, LPARAM, BOOL& bHandled) {
  bHandled = TRUE;
  return 0;
}

std::wstring CheckWindowsHotkeyConflict(const std::wstring& chord) {
  if (chord.empty())
    return L"";
  static const wchar_t* conflicts[] = {
      L"Alt+Tab",       L"Alt+Escape",    L"Control+Alt+Delete",
      L"Super",         L"Super+D",       L"Super+L",
      L"Super+E",       L"Super+R",       L"Super+Tab",
      L"Super+Shift+S", L"Control+Shift+Escape",
      L"Control+Escape"};
  std::wstring upper = chord;
  for (auto& c : upper)
    c = towupper(c);
  for (const auto* c : conflicts) {
    std::wstring cu = c;
    for (auto& ch : cu)
      ch = towupper(ch);
    if (upper == cu)
      return L"可能与系统快捷键冲突";
  }
  if (upper.find(L"SUPER") != std::wstring::npos)
    return L"含 Win 键，可能与系统冲突";
  if (upper == L"ALT+F4")
    return L"可能与关闭窗口冲突";
  return L"";
}
