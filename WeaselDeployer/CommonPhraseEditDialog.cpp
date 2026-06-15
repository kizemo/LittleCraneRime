#include "stdafx.h"
#include "CommonPhraseEditDialog.h"
#include "DeployerUiHelper.h"
#include <CommonPhraseStore.h>
#include <WeaselUiTheme.h>

namespace {
constexpr int kMargin = 16;
constexpr int kBtnW = 72;
constexpr int kBtnH = 28;
constexpr int kGap = 8;
constexpr int kMinListH = 180;
constexpr int kMinInputH = 96;
constexpr int kDefaultW = 520;
constexpr int kDefaultH = 500;
constexpr int kMinW = 440;
constexpr int kMinH = 400;
constexpr wchar_t kGeomKey[] = L"CommonPhraseEdit";
}  // namespace

CommonPhraseEditDialog::CommonPhraseEditDialog() {}

CommonPhraseEditDialog::~CommonPhraseEditDialog() {
  if (font_normal_)
    DeleteObject(font_normal_);
  if (font_title_)
    DeleteObject(font_title_);
}

void CommonPhraseEditDialog::BuildUi() {
  if (ui_built_)
    return;
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  font_normal_ = weasel_ui::CreateThemeFont(10, dpi);
  font_title_ = weasel_ui::CreateThemeFont(11, dpi, true);

  list_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
               WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY |
                   LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
               0, IDC_PHRASE_LIST);
  list_.SetFont(font_normal_);

  input_edit_.Create(m_hWnd, CRect(0, 0, 0, 0), NULL,
                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
                         ES_AUTOVSCROLL | ES_WANTRETURN);
  input_edit_.SetFont(font_normal_);

  auto make_btn = [&](CButton& btn, int id, const wchar_t* text) {
    btn.Create(m_hWnd, CRect(0, 0, 0, 0), text,
               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, id);
    btn.SetFont(font_normal_);
  };
  make_btn(btn_add_, IDC_PHRASE_ADD, L"添加");
  make_btn(btn_edit_, IDC_PHRASE_EDIT, L"编辑");
  make_btn(btn_delete_, IDC_PHRASE_DELETE, L"删除");
  make_btn(btn_ok_, IDOK, L"确定");
  make_btn(btn_cancel_, IDCANCEL, L"取消");
  ui_built_ = true;
}

void CommonPhraseEditDialog::LayoutUi(int cx, int cy) {
  if (!ui_built_ || cx <= 0 || cy <= 0)
    return;
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto S = [&](int v) { return deployer_ui::Scale(v, dpi); };

  const int m = S(kMargin);
  const int gap = S(kGap);
  const int btn_w = S(kBtnW);
  const int btn_h = S(kBtnH);
  const int footer_h = btn_h + m;

  int y = m + deployer_ui::TitleBarHeight(dpi);
  const int w = cx - m * 2;
  const int list_h =
      (std::max)(S(kMinListH), cy - footer_h - S(kMinInputH) - gap * 2 - m);
  const int input_h = (std::max)(S(kMinInputH), cy - m - list_h - gap - footer_h);

  list_.SetWindowPos(NULL, m, y, w, list_h, SWP_NOZORDER);
  y += list_h + gap;
  input_edit_.SetWindowPos(NULL, m, y, w, input_h, SWP_NOZORDER);
  y += input_h + gap;

  btn_add_.SetWindowPos(NULL, m, y, btn_w, btn_h, SWP_NOZORDER);
  btn_edit_.SetWindowPos(NULL, m + btn_w + gap, y, btn_w, btn_h, SWP_NOZORDER);
  btn_delete_.SetWindowPos(NULL, m + (btn_w + gap) * 2, y, btn_w, btn_h,
                           SWP_NOZORDER);
  btn_cancel_.SetWindowPos(NULL, cx - m - btn_w, y, btn_w, btn_h, SWP_NOZORDER);
  btn_ok_.SetWindowPos(NULL, cx - m - btn_w * 2 - gap, y, btn_w, btn_h,
                       SWP_NOZORDER);
}

LRESULT CommonPhraseEditDialog::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL&) {
  RECT rc;
  GetClientRect(&rc);
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  deployer_ui::PaintDialogBackground(reinterpret_cast<HDC>(wParam), rc,
                                     L"常用短语", font_title_, dpi);
  return 1;
}

LRESULT CommonPhraseEditDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  weasel_ui::ApplyDialogChrome(m_hWnd);
  deployer_ui::EnableResizableFrame(m_hWnd);
  SetWindowTextW(L"常用短语");
  BuildUi();
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  deployer_ui::ApplySavedOrDefaultGeometry(
      m_hWnd, kGeomKey, deployer_ui::Scale(kDefaultW, dpi),
      deployer_ui::Scale(kDefaultH, dpi));
  weasel::common_phrase::LoadPhrases(phrases_);
  ReloadList();
  CRect rc;
  GetClientRect(&rc);
  LayoutUi(rc.Width(), rc.Height());
  deployer_ui::BringDialogToFront(m_hWnd);
  return TRUE;
}

LRESULT CommonPhraseEditDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
  deployer_ui::SaveWindowGeometry(kGeomKey, m_hWnd);
  return 0;
}

LRESULT CommonPhraseEditDialog::OnSize(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
  if (wParam == SIZE_MINIMIZED)
    return 0;
  LayoutUi(LOWORD(lParam), HIWORD(lParam));
  return 0;
}

LRESULT CommonPhraseEditDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam,
                                                BOOL&) {
  UINT dpi = deployer_ui::GetDpiForWindow(m_hWnd);
  auto* mi = reinterpret_cast<MINMAXINFO*>(lParam);
  mi->ptMinTrackSize.x = deployer_ui::Scale(kMinW, dpi);
  mi->ptMinTrackSize.y = deployer_ui::Scale(kMinH, dpi);
  return 0;
}

LRESULT CommonPhraseEditDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

void CommonPhraseEditDialog::ReloadList() {
  list_.ResetContent();
  for (const auto& phrase : phrases_)
    list_.AddString(phrase.c_str());
}

int CommonPhraseEditDialog::GetSelectedIndex() const {
  int sel = list_.GetCurSel();
  return sel == LB_ERR ? -1 : sel;
}

bool CommonPhraseEditDialog::SavePhrases() {
  return weasel::common_phrase::SavePhrases(phrases_);
}

LRESULT CommonPhraseEditDialog::OnListSelChange(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index >= 0 && index < static_cast<int>(phrases_.size()))
    input_edit_.SetWindowTextW(phrases_[index].c_str());
  return 0;
}

LRESULT CommonPhraseEditDialog::OnAdd(WORD, WORD, HWND, BOOL&) {
  wchar_t buf[4096] = {0};
  input_edit_.GetWindowTextW(buf, 4096);
  std::wstring phrase = buf;
  if (phrase.empty()) {
    input_edit_.SetFocus();
    return 0;
  }
  phrases_.push_back(phrase);
  SavePhrases();
  ReloadList();
  list_.SetCurSel(static_cast<int>(phrases_.size()) - 1);
  input_edit_.SetWindowTextW(L"");
  return 0;
}

LRESULT CommonPhraseEditDialog::OnEdit(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index < 0) {
    ::MessageBoxW(m_hWnd, L"请先选择要编辑的短语，在下方输入框修改后点「编辑」。",
                  L"常用短语", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  wchar_t buf[4096] = {0};
  input_edit_.GetWindowTextW(buf, 4096);
  std::wstring phrase = buf;
  if (phrase.empty())
    phrase = phrases_[index];
  phrases_[index] = phrase;
  SavePhrases();
  ReloadList();
  list_.SetCurSel(index);
  return 0;
}

LRESULT CommonPhraseEditDialog::OnDelete(WORD, WORD, HWND, BOOL&) {
  int index = GetSelectedIndex();
  if (index < 0) {
    ::MessageBoxW(m_hWnd, L"请先选择要删除的短语。", L"常用短语",
                  MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (::MessageBoxW(m_hWnd, L"确定删除选中的常用短语吗？", L"常用短语",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
    return 0;
  phrases_.erase(phrases_.begin() + index);
  SavePhrases();
  ReloadList();
  return 0;
}

LRESULT CommonPhraseEditDialog::OnOK(WORD, WORD, HWND, BOOL&) {
  SavePhrases();
  EndDialog(IDOK);
  return 0;
}

LRESULT CommonPhraseEditDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}
