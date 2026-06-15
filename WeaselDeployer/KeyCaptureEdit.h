#pragma once

#include <functional>
#include <string>

class KeyCaptureEdit : public CWindowImpl<KeyCaptureEdit, CEdit> {
 public:
  KeyCaptureEdit();
  void SetKey(const std::wstring& key);
  std::wstring GetKey() const;
  void SetOnChange(std::function<void()> fn);

  BEGIN_MSG_MAP(KeyCaptureEdit)
  MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocusMsg)
  MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocusMsg)
  MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDownMsg)
  MESSAGE_HANDLER(WM_SYSKEYDOWN, OnKeyDownMsg)
  MESSAGE_HANDLER(WM_CHAR, OnChar)
  END_MSG_MAP()

  LRESULT OnSetFocusMsg(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnKillFocusMsg(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnKeyDownMsg(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

 private:
  std::wstring FormatKey(UINT vk, UINT flags) const;
  std::wstring captured_;
  std::function<void()> on_change_;
};

std::wstring CheckWindowsHotkeyConflict(const std::wstring& chord);
