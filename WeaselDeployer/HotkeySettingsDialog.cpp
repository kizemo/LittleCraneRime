#include "stdafx.h"
#include "HotkeySettingsDialog.h"
#include "DeployerUiHelper.h"
#include <WeaselUiTheme.h>
#include <WeaselUtility.h>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace {
constexpr int kMargin = 12;
constexpr int kRowHeight = 26;
constexpr int kGroupHeight = 22;
constexpr int kHeaderHeight = 20;
constexpr int kLabelX = 20;
constexpr int kEditX = 168;
constexpr int kEditW = 176;
constexpr int kConflictX = 352;
constexpr int kConflictW = 140;
constexpr int kFooterH = 36;
constexpr int kDefaultW = 540;
constexpr int kDefaultH = 0;  // computed after build
constexpr int kMinW = 460;
constexpr int kMinH = 360;
constexpr wchar_t kGeomKey[] = L"HotkeyDlg";
constexpr COLORREF kGroupBg = weasel_ui::kGroupBg;
constexpr COLORREF kGroupText = weasel_ui::kTextPrimary;
constexpr COLORREF kHeaderText = weasel_ui::kTextSecondary;

fs::path HotkeyConfigPath() {
  return WeaselUserDataPath() / "weasel_hotkeys.yaml";
}

fs::path DefaultCustomPath() {
  return WeaselUserDataPath() / "default.custom.yaml";
}

void EnsureDefaultCustomInclude() {
  fs::path custom_path = DefaultCustomPath();
  std::string marker = "__include: weasel_hotkeys:/patch";
  std::string content;
  if (fs::exists(custom_path)) {
    std::ifstream in(custom_path, std::ios::binary);
    content.assign((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
    if (content.find(marker) != std::string::npos)
      return;
  }
  if (content.empty()) {
    content =
        "# Rime customization\n# encoding: utf-8\n\n"
        "patch:\n  __include: weasel_hotkeys:/patch\n";
  } else if (content.find("patch:") != std::string::npos) {
    auto pos = content.find("patch:");
    pos = content.find('\n', pos);
    content.insert(pos + 1, "  __include: weasel_hotkeys:/patch\n");
  } else {
    content += "\npatch:\n  __include: weasel_hotkeys:/patch\n";
  }
  std::ofstream out(custom_path, std::ios::binary | std::ios::trunc);
  out << content;
}
}  // namespace

std::string HotkeySettingsDialog::ExtractYamlValue(const std::string& content,
                                                   const std::string& key) {
  std::string pattern = key + ":";
  auto pos = content.find(pattern);
  if (pos == std::string::npos)
    return "";
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
  return value;
}

bool HotkeySettingsDialog::LoadSettings(HotkeySettings& settings) {
  settings = HotkeySettings();
  fs::path path = HotkeyConfigPath();
  if (!fs::exists(path))
    return true;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  auto load = [&](const char* key, std::wstring& dst) {
    std::string v = ExtractYamlValue(content, key);
    if (!v.empty())
      dst = u8tow(v);
  };
  load("toggle_ascii_l", settings.toggle_ascii_l);
  load("toggle_ascii_r", settings.toggle_ascii_r);
  load("caps_lock", settings.caps_lock);
  load("pick_second", settings.pick_second);
  load("pick_third", settings.pick_third);
  if (settings.pick_second == L"2")
    settings.pick_second = L"Shift_L";
  if (settings.pick_third == L"3")
    settings.pick_third = L"Shift_R";
  load("shift_l_candidate", settings.pick_second);
  load("shift_r_candidate", settings.pick_third);
  load("page_up", settings.page_up);
  load("page_down", settings.page_down);
  load("toggle_ascii_punct", settings.toggle_ascii_punct);
  load("toggle_full_shape", settings.toggle_full_shape);
  load("tab_next", settings.tab_next);
  load("tab_prev", settings.tab_prev);
  load("alt_left", settings.alt_left);
  load("alt_right", settings.alt_right);
  load("select_first", settings.select_first);
  load("select_last", settings.select_last);
  load("schema_next", settings.schema_next);
  load("common_phrase_panel", settings.common_phrase_panel);
  load("quick_bar_panel", settings.quick_bar_panel);
  if (settings.common_phrase_panel.empty())
    settings.common_phrase_panel = L"Alt+period";
  if (settings.quick_bar_panel.empty())
    settings.quick_bar_panel = L"Alt+comma";
  return true;
}

bool HotkeySettingsDialog::SaveSettings(const HotkeySettings& settings) {
  fs::path path = HotkeyConfigPath();
  if (!fs::exists(path.parent_path()))
    fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;

  auto w = [&](const std::wstring& s) { return wtou8(s); };

  out << "# 小狼毫快捷键自定义配置\n# encoding: utf-8\n\n"
         "customization:\n"
         "  distribution_code_name: Weasel\n"
         "  generator: \"Weasel Hotkey Settings\"\n"
      << "toggle_ascii_l: " << w(settings.toggle_ascii_l) << "\n"
      << "toggle_ascii_r: " << w(settings.toggle_ascii_r) << "\n"
      << "caps_lock: " << w(settings.caps_lock) << "\n"
      << "pick_second: " << w(settings.pick_second) << "\n"
      << "pick_third: " << w(settings.pick_third) << "\n"
      << "page_up: " << w(settings.page_up) << "\n"
      << "page_down: " << w(settings.page_down) << "\n"
      << "toggle_ascii_punct: " << w(settings.toggle_ascii_punct) << "\n"
      << "toggle_full_shape: " << w(settings.toggle_full_shape) << "\n"
      << "tab_next: " << w(settings.tab_next) << "\n"
      << "tab_prev: " << w(settings.tab_prev) << "\n"
      << "alt_left: " << w(settings.alt_left) << "\n"
      << "alt_right: " << w(settings.alt_right) << "\n"
      << "select_first: " << w(settings.select_first) << "\n"
      << "select_last: " << w(settings.select_last) << "\n"
      << "schema_next: " << w(settings.schema_next) << "\n"
      << "common_phrase_panel: " << w(settings.common_phrase_panel) << "\n"
      << "quick_bar_panel: " << w(settings.quick_bar_panel) << "\n\n"
      << "patch:\n";

  auto ascii_action = [](const std::wstring& key) -> std::string {
    // Shift toggle is handled in TSF (_ToggleAsciiMode); Rime must stay noop.
    if (key == L"Shift_L" || key == L"Shift_R")
      return "noop";
    return "commit_code";
  };
  if (!settings.toggle_ascii_l.empty())
    out << "  ascii_composer/switch_key/" << w(settings.toggle_ascii_l) << ": "
        << ascii_action(settings.toggle_ascii_l) << "\n";
  if (!settings.toggle_ascii_r.empty() &&
      settings.toggle_ascii_r != settings.toggle_ascii_l)
    out << "  ascii_composer/switch_key/" << w(settings.toggle_ascii_r) << ": "
        << ascii_action(settings.toggle_ascii_r) << "\n";
  if (!settings.caps_lock.empty())
    out << "  ascii_composer/switch_key/Caps_Lock: "
        << w(settings.caps_lock) << "\n";

  out << "  key_binder/select_first_character: "
      << w(settings.select_first) << "\n"
      << "  key_binder/select_last_character: "
      << w(settings.select_last) << "\n"
      << "  key_binder/bindings/@before 0:\n";

  auto emit_menu = [&](const std::wstring& accept, const char* send) {
    if (!accept.empty())
      out << "    - { when: has_menu, accept: " << w(accept)
          << ", send: " << send << " }\n";
  };
  emit_menu(settings.pick_second, "2");
  emit_menu(settings.pick_third, "3");
  emit_menu(settings.page_up, "Page_Up");
  emit_menu(settings.page_down, "Page_Down");

  auto emit_binding = [&](const char* when, const std::wstring& accept,
                          const char* action, const char* target = "") {
    if (accept.empty())
      return;
    out << "  key_binder/bindings/+:\n    - { when: " << when << ", "
        << action << ": " << target << ", accept: " << w(accept) << " }\n";
  };

  emit_binding("always", settings.toggle_ascii_punct, "toggle", "ascii_punct");
  emit_binding("always", settings.toggle_full_shape, "toggle", "full_shape");
  emit_binding("always", L"Control+space", "toggle", "ascii_mode");
  emit_binding("composing", settings.tab_next, "send", "Shift+Right");
  emit_binding("composing", settings.tab_prev, "send", "Shift+Left");
  emit_binding("composing", settings.alt_left, "send", "Shift+Left");
  emit_binding("composing", settings.alt_right, "send", "Shift+Right");
  if (!settings.schema_next.empty())
    emit_binding("always", settings.schema_next, "select", ".next");

  EnsureDefaultCustomInclude();
  return true;
}

bool HotkeySettingsDialog::EnsureDefaultHotkeysInstalled() {
  EnsureDefaultCustomInclude();
  if (!fs::exists(HotkeyConfigPath()))
    return SaveSettings(HotkeySettings());
  return true;
}

void HotkeySettingsDialog::UpdateConflictLabel(RowWidgets& row) {
  if (row.is_group_header || !row.key_edit)
    return;
  std::wstring warn = CheckWindowsHotkeyConflict(row.key_edit->GetKey());
  row.conflict_label.SetWindowTextW(warn.c_str());
}

void HotkeySettingsDialog::BuildUi() {
  if (ui_built_)
    return;
  struct RowDef {
    const wchar_t* group;
    const wchar_t* action;
    std::wstring HotkeySettings::* field;
  };
  static const RowDef defs[] = {
      {L"基础", L"切换中英文", &HotkeySettings::toggle_ascii_l},
      {nullptr, L"切换中英文（副键）", &HotkeySettings::toggle_ascii_r},
      {nullptr, L"大写锁定", &HotkeySettings::caps_lock},
      {L"候选", L"选择第 2 候选", &HotkeySettings::pick_second},
      {nullptr, L"选择第 3 候选", &HotkeySettings::pick_third},
      {nullptr, L"上一页", &HotkeySettings::page_up},
      {nullptr, L"下一页", &HotkeySettings::page_down},
      {L"常用", L"打开常用短语", &HotkeySettings::common_phrase_panel},
      {nullptr, L"打开快捷设置栏", &HotkeySettings::quick_bar_panel},
      {L"模式", L"中英标点", &HotkeySettings::toggle_ascii_punct},
      {nullptr, L"全半角", &HotkeySettings::toggle_full_shape},
      {nullptr, L"切换输入方案", &HotkeySettings::schema_next},
      {L"编辑", L"下一音节", &HotkeySettings::tab_next},
      {nullptr, L"上一音节", &HotkeySettings::tab_prev},
      {nullptr, L"光标前移音节", &HotkeySettings::alt_left},
      {nullptr, L"光标后移音节", &HotkeySettings::alt_right},
      {nullptr, L"以词定字（首）", &HotkeySettings::select_first},
      {nullptr, L"以词定字（末）", &HotkeySettings::select_last},
  };

  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto S = [&](int v) { return deployer_ui::Scale(v, dpi); };
  font_normal_ = deployer_ui::CreateUiFont(10, dpi);
  font_bold_ = deployer_ui::CreateUiFont(10, dpi, true);
  font_title_ = weasel_ui::CreateThemeFont(11, dpi, true);
  group_brush_ = CreateSolidBrush(kGroupBg);
  surface_brush_ = CreateSolidBrush(weasel_ui::kSurface);

  int y = S(kMargin) + deployer_ui::TitleBarHeight(dpi);
  header_action_.Create(m_hWnd, CRect(0, 0, 0, 0), L"动作",
                        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
  header_action_.SetFont(font_bold_);
  header_key_.Create(m_hWnd, CRect(0, 0, 0, 0), L"快捷键",
                     WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
  header_key_.SetFont(font_bold_);
  header_hint_.Create(m_hWnd, CRect(0, 0, 0, 0), L"系统冲突",
                      WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
  header_hint_.SetFont(font_bold_);
  y += S(kHeaderHeight) + S(4);

  const wchar_t* last_group = L"";
  for (const auto& def : defs) {
    if (def.group && wcscmp(def.group, last_group) != 0) {
      RowWidgets group;
      group.is_group_header = true;
      group.label = def.group;
      group.name_label.Create(
          m_hWnd, CRect(0, 0, 0, 0), def.group,
          WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
      group.name_label.SetFont(font_bold_);
      rows_.push_back(std::move(group));
      y += S(kGroupHeight);
      last_group = def.group;
    }

    RowWidgets row;
    row.label = def.action;
    row.value = &(settings_.*(def.field));
    row.name_label.Create(m_hWnd, CRect(0, 0, 0, 0), def.action,
                          WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
    row.name_label.SetFont(font_normal_);

    row.key_edit = std::make_unique<KeyCaptureEdit>();
    row.key_edit->Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY);
    row.key_edit->SetFont(font_normal_);

    row.conflict_label.Create(m_hWnd, CRect(0, 0, 0, 0), L"",
                              WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
    row.conflict_label.SetFont(font_normal_);

    rows_.push_back(std::move(row));
    size_t idx = rows_.size() - 1;
    rows_[idx].key_edit->SetOnChange([this, idx]() {
      UpdateConflictLabel(rows_[idx]);
    });
    y += S(kRowHeight);
  }

  status_label_.Create(m_hWnd, CRect(0, 0, 0, 0),
      L"点击快捷键列，直接按键设置；Esc 清空，留空表示禁用。",
      WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
  status_label_.SetFont(font_normal_);

  btn_ok_.Attach(GetDlgItem(IDOK));
  btn_cancel_.Attach(GetDlgItem(IDCANCEL));
  if (btn_ok_.m_hWnd) {
    btn_ok_.SetWindowTextW(L"保存");
    btn_ok_.SetFont(font_normal_);
  }
  if (btn_cancel_.m_hWnd) {
    btn_cancel_.SetFont(font_normal_);
  }

  content_height_ = y + S(kFooterH) + S(kMargin) + deployer_ui::TitleBarHeight(dpi);
  ui_built_ = true;
}

void HotkeySettingsDialog::LayoutUi(int cx, int cy) {
  if (!ui_built_ || cx <= 0 || cy <= 0)
    return;
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto S = [&](int v) { return deployer_ui::Scale(v, dpi); };

  const int m = S(kMargin);
  const int row_h = S(kRowHeight);
  const int group_h = S(kGroupHeight);
  const int header_h = S(kHeaderHeight);
  const int footer_h = S(kFooterH);
  const int label_x = S(kLabelX);
  const int edit_x = S(kEditX);
  const int conflict_x = (std::max)(edit_x + S(120), cx - m - S(kConflictW));
  const int edit_w = (std::max)(S(100), conflict_x - edit_x - S(8));
  const int conflict_w = cx - m - conflict_x;
  const int btn_w = S(72);
  const int btn_h = S(28);

  int y = m + deployer_ui::TitleBarHeight(dpi);
  header_action_.SetWindowPos(NULL, label_x, y, edit_x - label_x - S(8), header_h,
                             SWP_NOZORDER);
  header_key_.SetWindowPos(NULL, edit_x, y, edit_w, header_h, SWP_NOZORDER);
  header_hint_.SetWindowPos(NULL, conflict_x, y, conflict_w, header_h,
                            SWP_NOZORDER);
  y += header_h + S(4);

  for (auto& row : rows_) {
    if (row.is_group_header) {
      row.name_label.SetWindowPos(NULL, m, y, cx - m * 2, group_h, SWP_NOZORDER);
      y += group_h;
      continue;
    }
    row.name_label.SetWindowPos(NULL, label_x, y, edit_x - label_x - S(8), row_h,
                                SWP_NOZORDER);
    if (row.key_edit)
      row.key_edit->SetWindowPos(NULL, edit_x, y, edit_w, row_h, SWP_NOZORDER);
    row.conflict_label.SetWindowPos(NULL, conflict_x, y, conflict_w, row_h,
                                    SWP_NOZORDER);
    y += row_h;
  }

  const int btn_y = cy - m - btn_h;
  const int status_y = btn_y - S(22) - S(4);
  status_label_.SetWindowPos(NULL, m, status_y, cx - m * 2, S(20), SWP_NOZORDER);
  if (btn_cancel_.m_hWnd)
    btn_cancel_.SetWindowPos(NULL, cx - m - btn_w, btn_y, btn_w, btn_h,
                             SWP_NOZORDER);
  if (btn_ok_.m_hWnd)
    btn_ok_.SetWindowPos(NULL, cx - m - btn_w * 2 - S(8), btn_y, btn_w, btn_h,
                         SWP_NOZORDER);
}

void HotkeySettingsDialog::WriteToControls() {
  for (auto& row : rows_) {
    if (row.is_group_header || !row.key_edit)
      continue;
    row.key_edit->SetKey(*row.value);
    UpdateConflictLabel(row);
  }
}

void HotkeySettingsDialog::ReadFromControls() {
  for (auto& row : rows_) {
    if (row.is_group_header || !row.key_edit)
      continue;
    *row.value = row.key_edit->GetKey();
  }
}

LRESULT HotkeySettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  weasel_ui::ApplyDialogChrome(m_hWnd);
  deployer_ui::EnableResizableFrame(m_hWnd);
  SetWindowTextW(L"快捷键设置");
  LoadSettings(settings_);
  BuildUi();
  WriteToControls();
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  const int default_h =
      content_height_ > 0 ? content_height_ : deployer_ui::Scale(520, dpi);
  deployer_ui::ApplySavedOrDefaultGeometry(m_hWnd, kGeomKey, kDefaultW, default_h);
  CRect rc;
  GetClientRect(&rc);
  LayoutUi(rc.Width(), rc.Height());
  deployer_ui::BringDialogToFront(m_hWnd);
  return TRUE;
}

LRESULT HotkeySettingsDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
  deployer_ui::SaveWindowGeometry(kGeomKey, m_hWnd);
  if (font_normal_)
    DeleteObject(font_normal_);
  if (font_bold_)
    DeleteObject(font_bold_);
  if (font_title_)
    DeleteObject(font_title_);
  if (group_brush_)
    DeleteObject(group_brush_);
  if (surface_brush_)
    DeleteObject(surface_brush_);
  return 0;
}

LRESULT HotkeySettingsDialog::OnSize(UINT, WPARAM wParam, LPARAM lParam,
                                     BOOL&) {
  if (wParam == SIZE_MINIMIZED)
    return 0;
  LayoutUi(LOWORD(lParam), HIWORD(lParam));
  return 0;
}

LRESULT HotkeySettingsDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam,
                                              BOOL&) {
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto mi = reinterpret_cast<MINMAXINFO*>(lParam);
  mi->ptMinTrackSize.x = deployer_ui::Scale(kMinW, dpi);
  mi->ptMinTrackSize.y = deployer_ui::Scale(kMinH, dpi);
  if (content_height_ > 0)
    mi->ptMaxTrackSize.y = content_height_;
  return 0;
}

LRESULT HotkeySettingsDialog::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
  RECT rc;
  GetClientRect(&rc);
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  deployer_ui::PaintDialogBackground(reinterpret_cast<HDC>(wParam), rc,
                                     L"快捷键设置", font_title_, dpi);
  return 1;
}

LRESULT HotkeySettingsDialog::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM lParam,
                                               BOOL&) {
  HWND hwnd = reinterpret_cast<HWND>(lParam);
  for (const auto& row : rows_) {
    if (row.is_group_header && row.name_label.m_hWnd == hwnd) {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      SetBkColor(hdc, kGroupBg);
      SetTextColor(hdc, kGroupText);
      return reinterpret_cast<LRESULT>(group_brush_);
    }
  }
  if (hwnd == header_action_.m_hWnd || hwnd == header_key_.m_hWnd ||
      hwnd == header_hint_.m_hWnd) {
    HDC hdc = reinterpret_cast<HDC>(wParam);
    SetTextColor(hdc, kHeaderText);
  }
  return 0;
}

LRESULT HotkeySettingsDialog::OnOK(WORD, WORD, HWND, BOOL&) {
  ReadFromControls();
  if (!SaveSettings(settings_)) {
    ::MessageBoxW(m_hWnd, L"无法保存快捷键配置。", L"快捷键设置",
                  MB_OK | MB_ICONERROR);
    return 0;
  }
  ::MessageBoxW(m_hWnd,
                L"快捷键已保存。\n请通过托盘菜单「重新部署」使设置生效。",
                L"快捷键设置", MB_OK | MB_ICONINFORMATION);
  EndDialog(IDOK);
  return 0;
}

LRESULT HotkeySettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}
