#pragma once

#include <WeaselHotkeyConfig.h>

#include <windows.h>



namespace weasel {

namespace global_hotkey {



constexpr int kIdQuickBar = 0x5E01;

constexpr int kIdCommonPhrase = 0x5E02;



inline UINT VkFromToken(const std::wstring& token) {
  if (token.empty())
    return 0;
  if (token.size() == 1) {
    wchar_t c = towupper(token[0]);
    if (c >= L'A' && c <= L'Z')
      return c;
    if (c >= L'0' && c <= L'9')
      return c;
  }
  if (token.size() >= 2 && token[0] == L'f' && iswdigit(token[1])) {
    int n = _wtoi(token.c_str() + 1);
    if (n >= 1 && n <= 24)
      return VK_F1 + n - 1;
  }
  // 0.18.99: 标点符号 token (RegisterHotKey 友好的命名)
  if (token == L"comma")
    return VK_OEM_COMMA;
  if (token == L"period" || token == L"dot" || token == L"full_stop")
    return VK_OEM_PERIOD;
  if (token == L"semicolon")
    return VK_OEM_1;
  if (token == L"slash" || token == L"question")
    return VK_OEM_2;
  if (token == L"grave" || token == L"tilde")
    return VK_OEM_3;
  if (token == L"bracketleft" || token == L"brace_left")
    return VK_OEM_4;
  if (token == L"backslash" || token == L"bar")
    return VK_OEM_5;
  if (token == L"bracketright" || token == L"brace_right")
    return VK_OEM_6;
  if (token == L"apostrophe" || token == L"quote")
    return VK_OEM_7;
  if (token == L"minus" || token == L"dash")
    return VK_OEM_MINUS;
  if (token == L"equal" || token == L"plus")
    return VK_OEM_PLUS;
  return 0;
}



inline bool ParseChordToModVk(const std::wstring& chord, UINT& mod, UINT& vk) {

  mod = 0;

  vk = 0;

  if (chord.empty())

    return false;

  std::wstring s = chord;

  for (auto& c : s)

    c = towlower(c);

  size_t start = 0;

  while (start < s.size()) {

    size_t plus = s.find(L'+', start);

    std::wstring part =

        (plus == std::wstring::npos) ? s.substr(start) : s.substr(start, plus - start);

    if (part == L"control" || part == L"ctrl")

      mod |= MOD_CONTROL;

    else if (part == L"shift")

      mod |= MOD_SHIFT;

    else if (part == L"alt")

      mod |= MOD_ALT;

    else if (part == L"super" || part == L"win")

      mod |= MOD_WIN;

    else

      vk = VkFromToken(part);

    if (plus == std::wstring::npos)

      break;

    start = plus + 1;

  }

  return vk != 0;

}



inline void RegisterPanelHotkeys(HWND hwnd) {

  if (!hwnd)

    return;

  UnregisterHotKey(hwnd, kIdQuickBar);

  UnregisterHotKey(hwnd, kIdCommonPhrase);



  UINT mod = 0, vk = 0;

  if (ParseChordToModVk(hotkey_config::LoadQuickBarPanelHotkey(), mod, vk))

    RegisterHotKey(hwnd, kIdQuickBar, mod, vk);

  if (ParseChordToModVk(hotkey_config::LoadCommonPhrasePanelHotkey(), mod, vk))

    RegisterHotKey(hwnd, kIdCommonPhrase, mod, vk);

}



inline void UnregisterPanelHotkeys(HWND hwnd) {

  if (!hwnd)

    return;

  UnregisterHotKey(hwnd, kIdQuickBar);

  UnregisterHotKey(hwnd, kIdCommonPhrase);

}



}  // namespace global_hotkey

}  // namespace weasel

