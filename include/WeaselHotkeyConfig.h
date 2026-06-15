#pragma once
#include <KeyEvent.h>
#include <WeaselUtility.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace weasel {
namespace hotkey_config {

inline std::wstring NormalizeChord(std::wstring chord) {
  auto replace_all = [&](const wchar_t* from, const wchar_t* to) {
    size_t pos = 0;
    size_t from_len = wcslen(from);
    size_t to_len = wcslen(to);
    while ((pos = chord.find(from, pos)) != std::wstring::npos) {
      chord.replace(pos, from_len, to);
      pos += to_len;
    }
  };
  replace_all(L"Control_L", L"Control");
  replace_all(L"Control_R", L"Control");
  replace_all(L"Shift_L", L"Shift");
  replace_all(L"Shift_R", L"Shift");
  replace_all(L"Alt_L", L"Alt");
  replace_all(L"Alt_R", L"Alt");
  for (auto& c : chord)
    c = towlower(c);
  return chord;
}

inline std::wstring FormatVkChord(UINT vk, UINT mod) {
  std::wstring chord;
  if (mod & MOD_CONTROL)
    chord += L"Control+";
  if (mod & MOD_ALT)
    chord += L"Alt+";
  if (mod & MOD_SHIFT)
    chord += L"Shift+";
  if (mod & MOD_WIN)
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
    case VK_PRIOR:
      chord += L"Page_Up";
      break;
    case VK_NEXT:
      chord += L"Page_Down";
      break;
    case VK_OEM_COMMA:
      chord += L"comma";
      break;
    case VK_OEM_PERIOD:
      chord += L"period";
      break;
    case VK_OEM_1:
      chord += L"semicolon";
      break;
    case VK_OEM_2:
      chord += L"slash";
      break;
    case VK_OEM_3:
      chord += L"grave";
      break;
    case VK_OEM_4:
      chord += L"bracketleft";
      break;
    case VK_OEM_5:
      chord += L"backslash";
      break;
    case VK_OEM_6:
      chord += L"bracketright";
      break;
    case VK_OEM_7:
      chord += L"apostrophe";
      break;
    case VK_OEM_MINUS:
      chord += L"minus";
      break;
    case VK_OEM_PLUS:
      chord += L"equal";
      break;
    default:
      if (vk >= VK_F1 && vk <= VK_F24) {
        wchar_t buf[8];
        swprintf_s(buf, L"F%d", vk - VK_F1 + 1);
        chord += buf;
      } else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        chord += static_cast<wchar_t>(vk);
      } else {
        return L"";
      }
      break;
  }
  return chord;
}

inline std::wstring ExtractYamlValue(const std::string& content,
                                     const std::string& key) {
  std::string pattern = key + ":";
  auto pos = content.find(pattern);
  if (pos == std::string::npos)
    return L"";
  pos += pattern.size();
  while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t'))
    ++pos;
  auto end = content.find_first_of("\r\n", pos);
  if (end == std::string::npos)
    end = content.size();
  std::string value = content.substr(pos, end - pos);
  while (!value.empty() && (value.back() == ' ' || value.back() == '\r'))
    value.pop_back();
  if (!value.empty() && value.front() == '"' && value.back() == '"')
    value = value.substr(1, value.size() - 2);
  return value.empty() ? L"" : u8tow(value);
}

inline std::wstring LoadYamlHotkey(const char* key,
                                 const wchar_t* default_value) {
  static std::wstring cached_common = L"Alt+period";
  static std::wstring cached_quick = L"Alt+comma";
  static std::filesystem::file_time_type cached_mtime{};
  std::wstring* cached =
      (strcmp(key, "quick_bar_panel") == 0) ? &cached_quick : &cached_common;
  std::filesystem::path path = WeaselUserDataPath() / L"weasel_hotkeys.yaml";
  if (!std::filesystem::exists(path))
    return *cached;
  auto mtime = std::filesystem::last_write_time(path);
  if (mtime != cached_mtime) {
    std::ifstream in(path, std::ios::binary);
    if (in) {
      std::string content((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
      std::wstring common = ExtractYamlValue(content, "common_phrase_panel");
      std::wstring quick = ExtractYamlValue(content, "quick_bar_panel");
      // 0.18.99: 迁移用户的旧默认值 (Control+Shift+K / Control+Shift+F)
      // 如果用户的值与历史硬编码默认值一致，视为"未自定义"，
      // 使用新默认 (Alt+period / Alt+comma)；
      // 用户若显式自定义为其他值，保留用户的设置。
      if (!common.empty() && NormalizeChord(common) != L"control+shift+k")
        cached_common = common;
      if (!quick.empty() && NormalizeChord(quick) != L"control+shift+f")
        cached_quick = quick;
      cached_mtime = mtime;
    }
  }
  return *cached;
}

inline std::wstring LoadCommonPhrasePanelHotkey() {
  return LoadYamlHotkey("common_phrase_panel", L"Alt+period");
}

inline std::wstring LoadQuickBarPanelHotkey() {
  return LoadYamlHotkey("quick_bar_panel", L"Alt+comma");
}

inline bool MatchVkHotkey(UINT vk, UINT mod, const std::wstring& expected) {
  if (expected.empty())
    return false;
  std::wstring chord = FormatVkChord(vk, mod);
  if (chord.empty())
    return false;
  return NormalizeChord(chord) == NormalizeChord(expected);
}

}  // namespace hotkey_config
}  // namespace weasel
