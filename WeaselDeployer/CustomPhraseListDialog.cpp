#include "stdafx.h"
#include "CustomPhraseListDialog.h"
#include "CustomPhraseDialog.h"
#include "DeployerUiHelper.h"
#include <WeaselUiTheme.h>

namespace {
constexpr int kMargin = 14;
constexpr int kBtnW = 76;
constexpr int kBtnH = 28;
constexpr int kGap = 6;
constexpr int kDetailH = 132;
constexpr int kDetailMinH = 120;
constexpr int kDefaultW = 600;
constexpr int kDefaultH = 600;
constexpr int kMinW = 480;
constexpr int kMinH = 420;
constexpr wchar_t kGeomKey[] = L"DictMgr";
}  // namespace
#include <WeaselUtility.h>
#include <CommonPhraseStore.h>
#include <WeaselIPC.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <CommDlg.h>
#include <set>
#pragma warning(disable : 4005)
#include <rime_api.h>
#include <rime_levers_api.h>
#pragma warning(default : 4005)

namespace fs = std::filesystem;

namespace {
std::string PhraseFileHeader(const std::wstring& filename) {
  std::string db_name = "custom_phrase";
  if (filename.find(L"double") != std::wstring::npos)
    db_name = "custom_phrase_double";
  return std::string("# Rime table\n# coding: utf-8\n#@/db_name\t") + db_name +
         "\n#@/db_type\ttabledb\n#\n# 自定义短语（格式：词汇<Tab>编码）\n#\n";
}

std::wstring EntryKey(const CustomPhraseEntry& e) {
  return e.phrase + L"\t" + e.code;
}

bool ParsePhraseLineImpl(const std::string& line, CustomPhraseEntry& entry) {
  std::string trimmed = line;
  while (!trimmed.empty() &&
         (trimmed.back() == '\r' || trimmed.back() == '\n' ||
          trimmed.back() == ' ' || trimmed.back() == '\t'))
    trimmed.pop_back();
  if (trimmed.empty() || trimmed[0] == '#')
    return false;
  auto tab = trimmed.find('\t');
  if (tab == std::string::npos)
    return false;
  std::wstring col_a = u8tow(trimmed.substr(0, tab));
  std::wstring col_b = u8tow(trimmed.substr(tab + 1));
  auto tab2 = col_b.find(L'\t');
  if (tab2 != std::wstring::npos)
    col_b = col_b.substr(0, tab2);
  auto looks_like_code = [](const std::wstring& s) {
    if (s.empty())
      return false;
    for (wchar_t c : s) {
      if (!iswalnum(c) && c != L'_' && c != L'-' && c != L'\'' && c != L' ')
        return false;
    }
    return true;
  };
  if (looks_like_code(col_a) && !looks_like_code(col_b)) {
    entry.code = col_a;
    entry.phrase = col_b;
  } else {
    entry.phrase = col_a;
    entry.code = col_b;
  }
  return !entry.code.empty() && !entry.phrase.empty();
}

bool ParseLearnedPhraseLine(const std::string& line, CustomPhraseEntry& entry) {
  if (ParsePhraseLineImpl(line, entry))
    return true;
  std::string trimmed = line;
  while (!trimmed.empty() &&
         (trimmed.back() == '\r' || trimmed.back() == '\n' ||
          trimmed.back() == ' ' || trimmed.back() == '\t'))
    trimmed.pop_back();
  if (trimmed.empty() || trimmed[0] == '#')
    return false;
  entry.phrase = u8tow(trimmed);
  entry.code.clear();
  return !entry.phrase.empty();
}

void RefreshServerUserDictSnapshot() {
  weasel::Client client;
  if (!client.Connect())
    return;
  client.EndMaintenance();
  client.StartMaintenance();
}
}  // namespace

CustomPhraseListDialog::CustomPhraseListDialog() {}

CustomPhraseListDialog::~CustomPhraseListDialog() {
  if (font_normal_)
    DeleteObject(font_normal_);
  if (font_bold_)
    DeleteObject(font_bold_);
  if (font_caption_)
    DeleteObject(font_caption_);
  if (font_title_)
    DeleteObject(font_title_);
  if (group_brush_)
    DeleteObject(group_brush_);
  if (surface_brush_)
    DeleteObject(surface_brush_);
}

std::wstring CustomPhraseListDialog::GetPhraseFileName() const {
  if (!phrase_file_.empty())
    return phrase_file_;

  fs::path user_dir = WeaselUserDataPath();
  fs::path double_file = user_dir / L"custom_phrase_double.txt";
  fs::path full_file = user_dir / L"custom_phrase.txt";

  if (fs::exists(double_file) && fs::file_size(double_file) > 0)
    return double_file.wstring();

  fs::path default_custom = user_dir / L"default.custom.yaml";
  if (fs::exists(default_custom)) {
    std::ifstream in(default_custom);
    std::string line;
    while (std::getline(in, line)) {
      if (line.find("double_pinyin") != std::string::npos)
        return double_file.wstring();
    }
  }

  if (fs::exists(full_file))
    return full_file.wstring();
  return full_file.wstring();
}

bool CustomPhraseListDialog::ParsePhraseLine(const std::string& line,
                                             CustomPhraseEntry& entry) {
  return ParsePhraseLineImpl(line, entry);
}

void CustomPhraseListDialog::BuildUi() {
  if (ui_built_)
    return;
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  font_normal_ = weasel_ui::CreateThemeFont(10, dpi);
  font_bold_ = weasel_ui::CreateThemeFont(10, dpi, true);
  font_caption_ = weasel_ui::CreateThemeFont(9, dpi);
  font_title_ = weasel_ui::CreateThemeFont(11, dpi, true);
  group_brush_ = CreateSolidBrush(weasel_ui::kGroupBg);
  surface_brush_ = CreateSolidBrush(weasel_ui::kSurface);

  subtitle_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
                   WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 0, IDC_STATIC1);
  subtitle_.SetWindowTextW(
      L"管理固定短语与用户学习词条。\n"
      L"学习词条为输入频率记录（含固定词库已有词），非异常；可勾选精简显示。");
  subtitle_.SetFont(font_caption_);

  stats_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
                WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 0, IDC_PHRASE_STATS);
  stats_.SetFont(font_caption_);

  list_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
               WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY |
                   LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
               0, IDC_PHRASE_LIST);
  list_.SetFont(font_normal_);

  detail_label_.Create(m_hWnd, CRect(0, 0, 0, 0), L"编辑所选固定短语",
                       WS_CHILD | SS_LEFTNOWORDWRAP, 0, IDC_PHRASE_DETAIL_LABEL);
  detail_label_.SetFont(font_bold_);

  label_code_.Create(m_hWnd, CRect(0, 0, 0, 0), L"编码",
                     WS_CHILD | SS_LEFTNOWORDWRAP, 0, IDC_STATIC1);
  label_code_.SetFont(font_caption_);
  label_phrase_.Create(m_hWnd, CRect(0, 0, 0, 0), L"短语",
                        WS_CHILD | SS_LEFTNOWORDWRAP, 0, IDC_STATIC1);
  label_phrase_.SetFont(font_caption_);

  edit_code_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
                    WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 0, IDC_PHRASE_CODE);
  edit_phrase_.Create(
      m_hWnd, CRect(0, 0, 0, 0), NULL,
      WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
      0, IDC_PHRASE_TEXT);
  weasel_ui::StyleEdit(edit_code_.m_hWnd, font_normal_);
  weasel_ui::StyleEdit(edit_phrase_.m_hWnd, font_normal_);

  btn_apply_.Create(m_hWnd, CRect(0, 0, 0, 0), L"应用修改",
                    WS_CHILD | BS_PUSHBUTTON, 0, IDC_PHRASE_APPLY_EDIT);
  btn_apply_.SetFont(font_normal_);

  hide_single_check_.Create(
      m_hWnd, CRect(0, 0, 0, 0), L"精简显示：隐藏单字学习词",
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, IDC_PHRASE_HIDE_SINGLE);
  hide_single_check_.SetFont(font_caption_);
  hide_single_check_.SetCheck(hide_single_char_learned_ ? BST_CHECKED : BST_UNCHECKED);

  auto make_btn = [&](CButton& btn, int id, const wchar_t* text) {
    btn.Create(m_hWnd, CRect(0, 0, 0, 0), text,
               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, id);
    btn.SetFont(font_normal_);
  };
  make_btn(btn_add_, IDC_PHRASE_ADD, L"添加");
  make_btn(btn_edit_, IDC_PHRASE_EDIT, L"编辑");
  make_btn(btn_delete_, IDC_PHRASE_DELETE, L"删除");
  make_btn(btn_sync_, IDC_PHRASE_SYNC, L"刷新学习词");
  make_btn(btn_export_, IDC_PHRASE_EXPORT, L"导出");
  make_btn(btn_import_, IDC_PHRASE_IMPORT, L"导入");
  if (HWND close_hwnd = GetDlgItem(IDCANCEL)) {
    btn_close_.Attach(close_hwnd);
    btn_close_.SetWindowTextW(L"关闭");
    btn_close_.SetFont(font_normal_);
    btn_close_.ShowWindow(SW_SHOW);
  } else {
    make_btn(btn_close_, IDCANCEL, L"关闭");
  }

  ui_built_ = true;
  CRect rc;
  GetClientRect(&rc);
  LayoutUi(rc.Width(), rc.Height());
}

void CustomPhraseListDialog::LayoutUi(int cx, int cy) {
  if (!ui_built_ || cx <= 0 || cy <= 0)
    return;
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto S = [&](int v) { return deployer_ui::Scale(v, dpi); };

  const int m = S(kMargin);
  const int gap = S(kGap);
  const int btn_w = S(kBtnW);
  const int btn_h = S(kBtnH);
  const int footer_h = S(26) + gap + btn_h + gap + btn_h + m;
  int detail_h = 0;
  if (detail_visible_) {
    detail_h = edit_code_.IsWindowVisible() ? S(kDetailH) : S(42);
    const int min_content = m + S(36) + S(22) + S(80) + detail_h + footer_h + gap;
    if (cy > min_content)
      detail_h += cy - min_content;
  }

  int y = m;
  const int w = cx - m * 2;

  subtitle_.SetWindowPos(NULL, m, y, w, S(34), SWP_NOZORDER);
  y += S(36);
  stats_.SetWindowPos(NULL, m, y, w, S(18), SWP_NOZORDER);
  y += S(22);

  const int list_bottom = cy - footer_h - detail_h - (detail_visible_ ? gap : 0);
  const int list_h = (std::max)(S(80), list_bottom - y);
  list_.SetWindowPos(NULL, m, y, w, list_h, SWP_NOZORDER);
  y = list_bottom;

  const int detail_show = detail_visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
  detail_label_.SetWindowPos(NULL, m, y + S(2), w, S(18), SWP_NOZORDER | detail_show);
  label_code_.SetWindowPos(NULL, m, y + S(22), S(36), S(18),
                           SWP_NOZORDER | detail_show);
  edit_code_.SetWindowPos(NULL, m + S(40), y + S(20), w - S(40), S(22),
                          SWP_NOZORDER | detail_show);
  label_phrase_.SetWindowPos(NULL, m, y + S(46), S(36), S(18),
                             SWP_NOZORDER | detail_show);
  const int apply_row_y = y + detail_h - btn_h - S(6);
  const int phrase_y = y + S(44);
  const int phrase_h = (std::max)(S(56), apply_row_y - phrase_y - S(4));
  edit_phrase_.SetWindowPos(NULL, m + S(40), phrase_y, w - S(40), phrase_h,
                            SWP_NOZORDER | detail_show);
  btn_apply_.SetWindowPos(NULL, m + w - btn_w, apply_row_y, btn_w, btn_h,
                          SWP_NOZORDER | detail_show);
  if (detail_visible_)
    y += detail_h + gap;

  hide_single_check_.SetWindowPos(NULL, m, y, w, S(22), SWP_NOZORDER);
  y += S(26) + gap;

  const int row_w = btn_w * 3 + gap * 2;
  btn_add_.SetWindowPos(NULL, m, y, btn_w, btn_h, SWP_NOZORDER);
  btn_edit_.SetWindowPos(NULL, m + btn_w + gap, y, btn_w, btn_h, SWP_NOZORDER);
  btn_delete_.SetWindowPos(NULL, m + (btn_w + gap) * 2, y, btn_w, btn_h,
                          SWP_NOZORDER);
  btn_sync_.SetWindowPos(NULL, m + row_w + gap, y, btn_w + S(16), btn_h,
                         SWP_NOZORDER);
  y += btn_h + gap;

  btn_export_.SetWindowPos(NULL, m, y, btn_w, btn_h, SWP_NOZORDER);
  btn_import_.SetWindowPos(NULL, m + btn_w + gap, y, btn_w, btn_h, SWP_NOZORDER);
  btn_close_.SetWindowPos(NULL, cx - m - btn_w, y, btn_w, btn_h, SWP_NOZORDER);
}

void CustomPhraseListDialog::UpdateDetailPanel() {
  int index = GetSelectedIndex();
  const bool fixed = index >= 0 && !IsLearnedSelection(index);
  const bool learned = index >= 0 && IsLearnedSelection(index);
  detail_visible_ = fixed || learned;

  if (fixed) {
    const auto& entry = display_entries_[index];
    detail_label_.SetWindowTextW(L"编辑所选固定短语");
    detail_label_.ShowWindow(SW_SHOW);
    label_code_.ShowWindow(SW_SHOW);
    label_phrase_.ShowWindow(SW_SHOW);
    edit_code_.ShowWindow(SW_SHOW);
    edit_phrase_.ShowWindow(SW_SHOW);
    btn_apply_.ShowWindow(SW_SHOW);
    edit_code_.SetWindowTextW(entry.code.c_str());
    edit_phrase_.SetWindowTextW(entry.phrase.c_str());
    edit_code_.EnableWindow(TRUE);
    edit_phrase_.EnableWindow(TRUE);
    btn_apply_.EnableWindow(TRUE);
  } else if (learned) {
    detail_label_.SetWindowTextW(
        L"学习词条：请在输入时右键删除，或点「刷新学习词」后导出管理");
    detail_label_.ShowWindow(SW_SHOW);
    label_code_.ShowWindow(SW_HIDE);
    label_phrase_.ShowWindow(SW_HIDE);
    edit_code_.ShowWindow(SW_HIDE);
    edit_phrase_.ShowWindow(SW_HIDE);
    btn_apply_.ShowWindow(SW_HIDE);
  } else {
    detail_label_.ShowWindow(SW_HIDE);
    label_code_.ShowWindow(SW_HIDE);
    label_phrase_.ShowWindow(SW_HIDE);
    edit_code_.ShowWindow(SW_HIDE);
    edit_phrase_.ShowWindow(SW_HIDE);
    btn_apply_.ShowWindow(SW_HIDE);
  }

  CRect rc;
  GetClientRect(&rc);
  LayoutUi(rc.Width(), rc.Height());
}

bool CustomPhraseListDialog::ShouldShowLearnedEntry(
    const CustomPhraseEntry& entry) const {
  if (!hide_single_char_learned_)
    return true;
  return entry.phrase.size() > 1;
}

void CustomPhraseListDialog::PopulateListBox() {
  display_entries_.clear();
  list_.ResetContent();
  for (const auto& entry : custom_entries_) {
    display_entries_.push_back(entry);
    std::wstring label = L"[固定] " + entry.code + L"  →  " + entry.phrase;
    list_.AddString(label.c_str());
  }
  for (const auto& entry : learned_entries_) {
    if (!ShouldShowLearnedEntry(entry))
      continue;
    display_entries_.push_back(entry);
    std::wstring label = L"[学习] " + entry.code + L"  →  " + entry.phrase;
    list_.AddString(label.c_str());
  }
  UpdateStatsLabel();
  UpdateDetailPanel();
}

void CustomPhraseListDialog::UpdateStatsLabel() {
  if (!stats_.m_hWnd)
    return;
  int shown_learned = 0;
  for (const auto& entry : learned_entries_) {
    if (ShouldShowLearnedEntry(entry))
      ++shown_learned;
  }
  wchar_t buf[192];
  swprintf_s(buf,
             L"固定短语 %u 条 · 学习词条 %u 条（显示 %u 条，共 %u 条）",
             static_cast<unsigned>(custom_entries_.size()),
             static_cast<unsigned>(learned_entries_.size()),
             static_cast<unsigned>(shown_learned),
             static_cast<unsigned>(learned_entries_.size()));
  stats_.SetWindowTextW(buf);
}

LRESULT CustomPhraseListDialog::OnCtlColorStatic(UINT, WPARAM wParam, LPARAM lParam,
                                                 BOOL&) {
  if (reinterpret_cast<HWND>(lParam) == subtitle_.m_hWnd) {
    HDC hdc = reinterpret_cast<HDC>(wParam);
    SetBkColor(hdc, weasel_ui::kGroupBg);
    SetTextColor(hdc, weasel_ui::kTextSecondary);
    return reinterpret_cast<LRESULT>(group_brush_);
  }
  return 0;
}

LRESULT CustomPhraseListDialog::OnToggleHideSingle(WORD, WORD, HWND, BOOL&) {
  hide_single_char_learned_ =
      hide_single_check_.GetCheck() == BST_CHECKED;
  PopulateListBox();
  return 0;
}

LRESULT CustomPhraseListDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  weasel_ui::ApplyDialogChrome(m_hWnd);
  deployer_ui::EnableDarkTitleBar(m_hWnd);
  deployer_ui::EnableResizableFrame(m_hWnd);
  SetWindowTextW(L"词典管理");
  BuildUi();
  deployer_ui::ApplySavedOrDefaultGeometry(m_hWnd, kGeomKey, kDefaultW,
                                           kDefaultH);
  phrase_file_ = GetPhraseFileName();
  ReloadList();
  deployer_ui::BringDialogToFront(m_hWnd);
  return TRUE;
}

LRESULT CustomPhraseListDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
  deployer_ui::SaveWindowGeometry(kGeomKey, m_hWnd);
  return 0;
}

LRESULT CustomPhraseListDialog::OnSize(UINT, WPARAM wParam, LPARAM lParam,
                                       BOOL&) {
  if (wParam == SIZE_MINIMIZED)
    return 0;
  LayoutUi(LOWORD(lParam), HIWORD(lParam));
  return 0;
}

LRESULT CustomPhraseListDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam,
                                                BOOL&) {
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto mi = reinterpret_cast<MINMAXINFO*>(lParam);
  mi->ptMinTrackSize.x = deployer_ui::Scale(kMinW, dpi);
  mi->ptMinTrackSize.y = deployer_ui::Scale(kMinH, dpi);
  return 0;
}

LRESULT CustomPhraseListDialog::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
  RECT rc;
  GetClientRect(&rc);
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  deployer_ui::PaintDialogBackground(reinterpret_cast<HDC>(wParam), rc, nullptr,
                                     nullptr, dpi);
  return 1;
}

LRESULT CustomPhraseListDialog::OnListSelChange(WORD, WORD, HWND, BOOL&) {
  UpdateDetailPanel();
  return 0;
}

LRESULT CustomPhraseListDialog::OnApplyEdit(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index < 0 || IsLearnedSelection(index))
    return 0;
  wchar_t code[256] = {};
  wchar_t phrase[2048] = {};
  edit_code_.GetWindowTextW(code, _countof(code));
  edit_phrase_.GetWindowTextW(phrase, _countof(phrase));
  if (!code[0] || !phrase[0]) {
    ::MessageBoxW(m_hWnd, L"编码和短语不能为空。", L"词典管理",
                  MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  const auto& selected = display_entries_[index];
  for (auto& entry : custom_entries_) {
    if (entry.code == selected.code && entry.phrase == selected.phrase) {
      entry.code = code;
      entry.phrase = phrase;
      break;
    }
  }
  if (SaveEntries()) {
    ReloadList();
    list_.SetCurSel(index);
    UpdateDetailPanel();
  } else {
    ::MessageBoxW(m_hWnd, L"无法保存词条文件。", L"词典管理",
                  MB_OK | MB_ICONERROR);
  }
  return 0;
}

LRESULT CustomPhraseListDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

namespace {

void LoadCustomPhraseFile(const fs::path& path,
                          std::vector<CustomPhraseEntry>& out,
                          std::set<std::wstring>& keys) {
  if (!fs::exists(path))
    return;
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    CustomPhraseEntry entry;
    if (!ParsePhraseLineImpl(line, entry))
      continue;
    entry.learned = false;
    entry.source_file = path.filename().wstring();
    std::wstring key = EntryKey(entry);
    if (keys.count(key))
      continue;
    keys.insert(key);
    out.push_back(entry);
  }
}

void ExportCustomPhraseFile(std::ofstream& out, const fs::path& path,
                            const wchar_t* section_title) {
  if (!fs::exists(path))
    return;
  out << "# --- " << wtou8(section_title) << " ---\n";
  out << PhraseFileHeader(path.filename().wstring());
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    CustomPhraseEntry entry;
    if (!ParsePhraseLineImpl(line, entry))
      continue;
    out << wtou8(entry.phrase) << "\t" << wtou8(entry.code) << "\n";
  }
  out << "\n";
}

}  // namespace

void CustomPhraseListDialog::ReloadList() {
  custom_entries_.clear();
  fs::path user_dir = WeaselUserDataPath();
  if (!fs::exists(user_dir))
    fs::create_directories(user_dir);
  std::set<std::wstring> keys;
  LoadCustomPhraseFile(user_dir / L"custom_phrase.txt", custom_entries_, keys);
  LoadCustomPhraseFile(user_dir / L"custom_phrase_double.txt", custom_entries_,
                       keys);
  ReloadUserDictEntries();
}

void CustomPhraseListDialog::ReloadUserDictEntries() {
  learned_entries_.clear();

  RimeModule* levers = rime_get_api()->find_module("levers");
  if (!levers) {
    PopulateListBox();
    return;
  }
  RimeLeversApi* api = (RimeLeversApi*)levers->get_api();
  if (!api || !api->export_user_dict) {
    PopulateListBox();
    return;
  }

  std::set<std::wstring> keys;
  auto load_export = [&](const char* dict_name) {
    fs::path tmp = fs::temp_directory_path() /
                   (std::wstring(L"weasel_user_dict_") + u8tow(dict_name) +
                    L".txt");
    const int n = api->export_user_dict(dict_name, wtou8(tmp.wstring()).c_str());
    if (n <= 0 || !fs::exists(tmp))
      return;
    std::ifstream in(tmp, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
      CustomPhraseEntry entry;
      if (!ParsePhraseLine(line, entry))
        continue;
      entry.learned = true;
      std::wstring key = EntryKey(entry);
      if (keys.count(key))
        continue;
      keys.insert(key);
      learned_entries_.push_back(entry);
    }
  };

  load_export("rime_ice");
  if (learned_entries_.empty() && api->user_dict_iterator_init) {
    RimeUserDictIterator iter = {0};
    api->user_dict_iterator_init(&iter);
    while (const char* dict = api->next_user_dict(&iter)) {
      if (dict && *dict)
        load_export(dict);
    }
    api->user_dict_iterator_destroy(&iter);
  }

  PopulateListBox();
}

bool CustomPhraseListDialog::SaveEntries() {
  fs::path user_dir = WeaselUserDataPath();
  const fs::path files[] = {user_dir / L"custom_phrase.txt",
                            user_dir / L"custom_phrase_double.txt"};
  for (const fs::path& path : files) {
    const std::wstring filename = path.filename().wstring();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out << PhraseFileHeader(filename);
    for (const auto& entry : custom_entries_) {
      const std::wstring& src =
          entry.source_file.empty() ? L"custom_phrase.txt" : entry.source_file;
      if (src != filename)
        continue;
      out << wtou8(entry.phrase) << "\t" << wtou8(entry.code) << "\n";
    }
  }
  RimeApi* api = rime_get_api();
  if (api)
    api->deploy();
  return true;
}

int CustomPhraseListDialog::GetSelectedIndex() const {
  int sel = list_.GetCurSel();
  if (sel == LB_ERR)
    return -1;
  return sel;
}

bool CustomPhraseListDialog::IsLearnedSelection(int index) const {
  if (index < 0 || index >= static_cast<int>(display_entries_.size()))
    return false;
  return display_entries_[index].learned;
}

LRESULT CustomPhraseListDialog::OnAdd(WORD, WORD, HWND, BOOL&) {
  CustomPhraseDialog dlg;
  if (dlg.DoModal() == IDOK) {
    CustomPhraseEntry entry;
    entry.code = dlg.GetCode();
    entry.phrase = dlg.GetPhrase();
    entry.source_file = L"custom_phrase.txt";
    if (!entry.code.empty() && !entry.phrase.empty()) {
      custom_entries_.push_back(entry);
      if (SaveEntries())
        ReloadList();
      else
        ::MessageBoxW(m_hWnd, L"无法保存词条文件。", L"词典管理",
                      MB_OK | MB_ICONERROR);
    }
  }
  return 0;
}

LRESULT CustomPhraseListDialog::OnEdit(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index < 0) {
    ::MessageBoxW(m_hWnd, L"请先选择要编辑的词条。", L"词典管理",
                  MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (IsLearnedSelection(index)) {
    ::MessageBoxW(m_hWnd,
                  L"用户学习词条请在输入时通过候选词右键删除，"
                  L"或导出后在用户词典目录管理。",
                  L"词典管理", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  UpdateDetailPanel();
  edit_code_.SetFocus();
  return 0;
}

LRESULT CustomPhraseListDialog::OnDelete(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index < 0) {
    ::MessageBoxW(m_hWnd, L"请先选择要删除的词条。", L"词典管理",
                  MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (IsLearnedSelection(index)) {
    ::MessageBoxW(m_hWnd,
                  L"学习词条请在上屏候选时右键「删除该候选词」。",
                  L"词典管理", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (::MessageBoxW(m_hWnd, L"确定删除选中的固定短语吗？", L"词典管理",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
    return 0;
  const auto& selected = display_entries_[index];
  for (auto it = custom_entries_.begin(); it != custom_entries_.end(); ++it) {
    if (it->code == selected.code && it->phrase == selected.phrase) {
      custom_entries_.erase(it);
      break;
    }
  }
  SaveEntries();
  ReloadList();
  return 0;
}

namespace {

enum class ImportSection { kNone, kCustomFull, kCustomDouble, kCommon, kLearned };

bool MergePhraseEntriesToFile(const fs::path& path,
                              const std::vector<CustomPhraseEntry>& incoming,
                              int* added) {
  std::set<std::wstring> keys;
  std::vector<CustomPhraseEntry> merged;
  if (fs::exists(path)) {
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
      CustomPhraseEntry entry;
      if (!ParsePhraseLineImpl(line, entry))
        continue;
      std::wstring key = EntryKey(entry);
      if (keys.count(key))
        continue;
      keys.insert(key);
      merged.push_back(entry);
    }
  }
  int local_added = 0;
  for (const auto& entry : incoming) {
    std::wstring key = EntryKey(entry);
    if (keys.count(key))
      continue;
    keys.insert(key);
    merged.push_back(entry);
    ++local_added;
  }
  if (local_added == 0)
    return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out << PhraseFileHeader(path.filename().wstring());
  for (const auto& entry : merged)
    out << wtou8(entry.phrase) << "\t" << wtou8(entry.code) << "\n";
  if (added)
    *added += local_added;
  return true;
}

}  // namespace

void CustomPhraseListDialog::MergeImportedEntries(const std::wstring& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    ::MessageBoxW(m_hWnd, L"无法打开导入文件。", L"词典管理", MB_OK | MB_ICONERROR);
    return;
  }

  ImportSection section = ImportSection::kNone;
  std::vector<CustomPhraseEntry> full_entries;
  std::vector<CustomPhraseEntry> double_entries;
  std::vector<std::wstring> common_entries;
  std::vector<CustomPhraseEntry> learned_entries;
  std::string line;
  while (std::getline(in, line)) {
    std::string trimmed = line;
    while (!trimmed.empty() &&
           (trimmed.back() == '\r' || trimmed.back() == '\n' ||
            trimmed.back() == ' ' || trimmed.back() == '\t'))
      trimmed.pop_back();
    if (trimmed.find("custom_phrase_double.txt") != std::string::npos) {
      section = ImportSection::kCustomDouble;
      continue;
    }
    if (trimmed.find("custom_phrase.txt") != std::string::npos) {
      section = ImportSection::kCustomFull;
      continue;
    }
    if (trimmed.find("weasel_common_phrases.txt") != std::string::npos) {
      section = ImportSection::kCommon;
      continue;
    }
    if (trimmed.find("rime_ice.userdb") != std::string::npos) {
      section = ImportSection::kLearned;
      continue;
    }
    if (trimmed.empty() || trimmed[0] == '#')
      continue;

    if (section == ImportSection::kCommon) {
      common_entries.push_back(u8tow(trimmed));
      continue;
    }

    CustomPhraseEntry entry;
    if (section == ImportSection::kLearned) {
      if (!ParseLearnedPhraseLine(trimmed, entry))
        continue;
      entry.learned = true;
      learned_entries.push_back(entry);
    } else if (!ParsePhraseLineImpl(trimmed, entry)) {
      continue;
    } else if (section == ImportSection::kCustomDouble) {
      entry.learned = false;
      double_entries.push_back(entry);
    } else if (section == ImportSection::kCustomFull) {
      entry.learned = false;
      full_entries.push_back(entry);
    } else {
      entry.learned = false;
      full_entries.push_back(entry);
    }
  }

  int added_custom = 0;
  int added_common = 0;
  int added_learned = 0;
  int skipped_learned_no_code = 0;
  fs::path user_dir = WeaselUserDataPath();
  if (!fs::exists(user_dir))
    fs::create_directories(user_dir);

  if (!full_entries.empty())
    MergePhraseEntriesToFile(user_dir / L"custom_phrase.txt", full_entries,
                             &added_custom);
  if (!double_entries.empty()) {
    MergePhraseEntriesToFile(user_dir / L"custom_phrase_double.txt",
                             double_entries, &added_custom);
    MergePhraseEntriesToFile(user_dir / L"custom_phrase.txt", double_entries,
                             &added_custom);
  }

  if (!common_entries.empty()) {
    std::vector<std::wstring> existing;
    weasel::common_phrase::LoadPhrases(existing);
    std::set<std::wstring> keys(existing.begin(), existing.end());
    for (const auto& phrase : common_entries) {
      if (keys.count(phrase))
        continue;
      keys.insert(phrase);
      existing.push_back(phrase);
      ++added_common;
    }
    if (added_common > 0)
      weasel::common_phrase::SavePhrases(existing);
  }

  if (!learned_entries.empty()) {
    RimeModule* levers = rime_get_api()->find_module("levers");
    if (levers) {
      RimeLeversApi* api = (RimeLeversApi*)levers->get_api();
      if (api && api->import_user_dict) {
        fs::path tmp =
            fs::temp_directory_path() / L"weasel_import_learned_dict.txt";
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << "# Rime user dictionary import\n";
        int importable = 0;
        for (const auto& entry : learned_entries) {
          if (entry.code.empty()) {
            ++skipped_learned_no_code;
            continue;
          }
          out << wtou8(entry.phrase) << "\t" << wtou8(entry.code) << "\t1\n";
          ++importable;
        }
        out.close();
        if (importable > 0) {
          added_learned =
              api->import_user_dict("rime_ice", wtou8(tmp.wstring()).c_str());
        }
      }
    }
  }

  if (!full_entries.empty() || !double_entries.empty()) {
    RimeApi* api = rime_get_api();
    if (api)
      api->deploy();
  }

  ReloadList();
  wchar_t msg[256];
  if (skipped_learned_no_code > 0) {
    swprintf_s(msg,
               L"导入完成：固定短语 %d 条，常用短语 %d 条，学习词条 %d 条。\n"
               L"跳过 %d 条学习词（格式须为：词汇<Tab>编码，例如：段燚<Tab>duan "
               L"yi）。",
               added_custom, added_common, added_learned,
               skipped_learned_no_code);
  } else {
    swprintf_s(msg,
               L"导入完成：固定短语 %d 条，常用短语 %d 条，学习词条 %d 条。",
               added_custom, added_common, added_learned);
  }
  ::MessageBoxW(m_hWnd, msg, L"词典管理", MB_OK | MB_ICONINFORMATION);
}

LRESULT CustomPhraseListDialog::OnExport(WORD, WORD, HWND, BOOL&) {
  wchar_t path[MAX_PATH] = L"dict_export.txt";
  OPENFILENAMEW ofn = {sizeof(ofn)};
  ofn.hwndOwner = m_hWnd;
  ofn.lpstrFilter = L"文本文件\0*.txt\0所有文件\0*.*\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrTitle = L"导出词典";
  ofn.lpstrDefExt = L"txt";
  if (!GetSaveFileNameW(&ofn))
    return 0;

  try {
    RefreshServerUserDictSnapshot();
    ReloadUserDictEntries();
    fs::path user_dir = WeaselUserDataPath();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "# 小鹅词典导出\n# encoding: utf-8\n\n";
    ExportCustomPhraseFile(out, user_dir / L"custom_phrase.txt",
                           L"固定短语 (custom_phrase.txt)");
    ExportCustomPhraseFile(out, user_dir / L"custom_phrase_double.txt",
                           L"固定短语 (custom_phrase_double.txt)");
    out << "# --- 常用短语 (weasel_common_phrases.txt) ---\n";
    out << "# 每行一条，无需编码\n#\n";
    std::vector<std::wstring> common_phrases;
    weasel::common_phrase::LoadPhrases(common_phrases);
    for (const auto& phrase : common_phrases)
      out << wtou8(phrase) << "\n";
    out << "\n# --- 用户学习词条 (rime_ice.userdb) ---\n";
    out << "# 格式：词汇<Tab>编码（多音节编码用空格分隔，例如：段燚<Tab>duan yi）\n";
    for (const auto& entry : learned_entries_)
      out << wtou8(entry.phrase) << "\t" << wtou8(entry.code) << "\n";
    wchar_t msg[160];
    swprintf_s(msg,
               L"导出成功：固定短语 %u 条，常用短语 %u 条，学习词条 %u 条。",
               static_cast<unsigned>(custom_entries_.size()),
               static_cast<unsigned>(common_phrases.size()),
               static_cast<unsigned>(learned_entries_.size()));
    ::MessageBoxW(m_hWnd, msg, L"词典管理", MB_OK | MB_ICONINFORMATION);
  } catch (...) {
    ::MessageBoxW(m_hWnd, L"导出失败。", L"词典管理", MB_OK | MB_ICONERROR);
  }
  return 0;
}

LRESULT CustomPhraseListDialog::OnImport(WORD, WORD, HWND, BOOL&) {
  wchar_t path[MAX_PATH] = {0};
  OPENFILENAMEW ofn = {sizeof(ofn)};
  ofn.hwndOwner = m_hWnd;
  ofn.lpstrFilter = L"文本文件\0*.txt\0所有文件\0*.*\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  ofn.lpstrTitle = L"导入词典（dict_export 格式）";
  if (!GetOpenFileNameW(&ofn))
    return 0;

  MergeImportedEntries(path);
  return 0;
}

LRESULT CustomPhraseListDialog::OnSyncUserDict(WORD, WORD, HWND, BOOL&) {
  RefreshServerUserDictSnapshot();
  ReloadUserDictEntries();
  wchar_t msg[192];
  swprintf_s(msg,
             L"已同步并刷新学习词条 %u 条。\n"
             L"说明：输入时选词上屏会自动写入用户词典；"
             L"本按钮用于将内存中的最新学习词落盘并更新列表。",
             static_cast<unsigned>(learned_entries_.size()));
  ::MessageBoxW(m_hWnd, msg, L"词典管理", MB_OK | MB_ICONINFORMATION);
  return 0;
}

LRESULT CustomPhraseListDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}
