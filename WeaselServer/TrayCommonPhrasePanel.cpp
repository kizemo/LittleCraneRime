#include "stdafx.h"

#include "TrayCommonPhrasePanel.h"

#include <CommonPhraseStore.h>
#include <WeaselUiTheme.h>
#include <ShellScalingApi.h>
#include <commctrl.h>

#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "comctl32.lib")

const wchar_t* TrayCommonPhrasePanel::kWndClass = L"WeaselTrayCommonPhrasePanel";
TrayCommonPhrasePanel* TrayCommonPhrasePanel::instance_ = nullptr;
TrayCommonPhrasePanel* TrayCommonPhrasePanel::hook_panel_ = nullptr;

#define IDC_PHRASE_LIST  2101
#define IDC_BTN_EDIT     2102
#define IDC_BTN_CLOSE    2103

namespace {
constexpr int kBaseWidth = 520;
constexpr int kBaseHeight = 420;
constexpr int kMinWidth = 380;
constexpr int kMinHeight = 280;
constexpr int kBasePadding = 12;
constexpr int kBaseHeader = 40;
constexpr int kBaseCornerRadius = 12;
constexpr BYTE kWindowAlpha = weasel_ui::kPanelAlpha;
constexpr wchar_t kUiRegistryKey[] = L"Software\\Rime\\Weasel\\UI";

UINT GetDpiForPoint(POINT pt) {
  HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  UINT dpiX = 96;
  UINT dpiY = 96;
  if (mon && SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
    return dpiX;
  HDC hdc = GetDC(NULL);
  if (hdc) {
    dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
  }
  return dpiX;
}

LRESULT CALLBACK PhrasePanelKeyboardHook(int nCode,
                                         WPARAM wParam,
                                         LPARAM lParam) {
  if (nCode == HC_ACTION && TrayCommonPhrasePanel::hook_panel_ &&
      TrayCommonPhrasePanel::hook_panel_->IsVisible()) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      const auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
      if (kbd) {
        switch (kbd->vkCode) {
          case VK_UP:
          case VK_DOWN:
          case VK_RETURN:
          case VK_ESCAPE:
            TrayCommonPhrasePanel::hook_panel_->HandleHookKey(kbd->vkCode);
            return 1;
        }
      }
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK PhraseListSubclassProc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wParam,
                                         LPARAM lParam,
                                         UINT_PTR,
                                         DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<TrayCommonPhrasePanel*>(ref_data);
  if (self && msg == WM_KEYDOWN) {
    switch (wParam) {
      case VK_RETURN: {
        int sel = static_cast<int>(SendMessage(hwnd, LB_GETCURSEL, 0, 0));
        if (sel == LB_ERR)
          sel = 0;
        self->OnListSelect(sel);
        return 0;
      }
      case VK_ESCAPE:
        self->Hide();
        return 0;
    }
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}
}  // namespace

TrayCommonPhrasePanel::TrayCommonPhrasePanel()
    : hwnd_(NULL),
      list_(NULL),
      btn_edit_(NULL),
      btn_close_(NULL),
      font_(NULL),
      font_title_(NULL),
      dpi_(96),
      client_w_(0),
      client_h_(0),
      dragging_(false),
      drag_offset_{0, 0},
      show_tick_(0),
      suppress_sel_change_(false),
      keyboard_hook_(NULL) {}

TrayCommonPhrasePanel::~TrayCommonPhrasePanel() {
  StopKeyboardHook();
  Hide();
  if (font_) {
    DeleteObject(font_);
    font_ = NULL;
  }
  if (font_title_) {
    DeleteObject(font_title_);
    font_title_ = NULL;
  }
}

void TrayCommonPhrasePanel::SetHandlers(CommitFn on_commit, EditFn on_edit) {
  on_commit_ = std::move(on_commit);
  on_edit_ = std::move(on_edit);
}

int TrayCommonPhrasePanel::Scale(int v) const {
  return MulDiv(v, dpi_, 96);
}

void TrayCommonPhrasePanel::UpdateDpiScale(POINT anchor) {
  dpi_ = GetDpiForPoint(anchor);
  if (font_) {
    DeleteObject(font_);
    font_ = NULL;
  }
  if (font_title_) {
    DeleteObject(font_title_);
    font_title_ = NULL;
  }
  font_ = weasel_ui::CreateThemeFont(13, dpi_, false);
  font_title_ = weasel_ui::CreateThemeFont(14, dpi_, true);
}

POINT TrayCommonPhrasePanel::NormalizeAnchor(POINT anchor) const {
  GetCursorPos(&anchor);
  using PTLPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
  auto PhysicalToLogical =
      (PTLPMDPI)GetProcAddress(GetModuleHandle(L"user32.dll"),
                               "PhysicalToLogicalPointForPerMonitorDPI");
  if (PhysicalToLogical)
    PhysicalToLogical(NULL, &anchor);
  return anchor;
}

void TrayCommonPhrasePanel::CreateWindowClass() {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = kWndClass;
  wc.hbrBackground = NULL;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  RegisterClassExW(&wc);
}

void TrayCommonPhrasePanel::ReloadPhrases() {
  weasel::common_phrase::LoadPhrases(phrases_);
  if (!list_)
    return;
  suppress_sel_change_ = true;
  SendMessage(list_, WM_SETREDRAW, FALSE, 0);
  SendMessage(list_, LB_RESETCONTENT, 0, 0);
  for (const auto& phrase : phrases_)
    SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(phrase.c_str()));
  SendMessage(list_, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(list_, NULL, TRUE);
  suppress_sel_change_ = false;
}

void TrayCommonPhrasePanel::CreateControls() {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  client_w_ = rc.right - rc.left;
  client_h_ = rc.bottom - rc.top;
  int pad = Scale(kBasePadding);
  int header = Scale(kBaseHeader);
  int width = client_w_;
  int btn_w = Scale(60);
  int btn_h = Scale(28);
  int close_w = Scale(32);

  btn_close_ = CreateWindowW(
      L"BUTTON", L"\x00D7", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      width - pad - btn_w - Scale(6) - close_w, pad + Scale(6), close_w, btn_h,
      hwnd_, (HMENU)(INT_PTR)IDC_BTN_CLOSE, GetModuleHandle(NULL), NULL);

  btn_edit_ = CreateWindowW(
      L"BUTTON", L"编辑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      width - pad - btn_w, pad + Scale(6), btn_w, btn_h, hwnd_,
      (HMENU)(INT_PTR)IDC_BTN_EDIT, GetModuleHandle(NULL), NULL);

  list_ = CreateWindowW(
      L"LISTBOX", NULL,
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS |
          LBS_NOINTEGRALHEIGHT,
      pad, pad + header, width - pad * 2,
      client_h_ - pad * 2 - header, hwnd_,
      (HMENU)(INT_PTR)IDC_PHRASE_LIST, GetModuleHandle(NULL), NULL);

  weasel_ui::StyleButton(btn_edit_, font_);
  if (btn_close_)
    weasel_ui::StyleButton(btn_close_, font_);
  weasel_ui::StyleEdit(list_, font_);
  SetWindowSubclass(list_, PhraseListSubclassProc, 1,
                    reinterpret_cast<DWORD_PTR>(this));
  ReloadPhrases();
}

void TrayCommonPhrasePanel::LayoutControls() {
  if (!hwnd_)
    return;
  RECT rc;
  GetClientRect(hwnd_, &rc);
  client_w_ = rc.right - rc.left;
  client_h_ = rc.bottom - rc.top;
  if (client_w_ <= 0 || client_h_ <= 0)
    return;

  int pad = Scale(kBasePadding);
  int header = Scale(kBaseHeader);
  int width = client_w_;
  int height = client_h_;
  int btn_w = Scale(60);
  int btn_h = Scale(28);
  int close_w = Scale(32);

  if (btn_close_)
    SetWindowPos(btn_close_, NULL, width - pad - btn_w - Scale(6) - close_w,
                 pad + Scale(6), close_w, btn_h, SWP_NOZORDER);
  SetWindowPos(btn_edit_, NULL, width - pad - btn_w, pad + Scale(6), btn_w,
               btn_h, SWP_NOZORDER);
  SetWindowPos(list_, NULL, pad, pad + header, width - pad * 2,
               height - pad * 2 - header, SWP_NOZORDER);
}

void TrayCommonPhrasePanel::SaveGeometry() {
  if (!hwnd_)
    return;
  RECT wr;
  GetWindowRect(hwnd_, &wr);
  const int x = wr.left;
  const int y = wr.top;
  const int w = wr.right - wr.left;
  const int h = wr.bottom - wr.top;
  HKEY key = NULL;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, NULL, 0, KEY_WRITE,
                      NULL, &key, NULL) != ERROR_SUCCESS)
    return;
  auto write = [&](const wchar_t* name, int value) {
    DWORD v = static_cast<DWORD>(value);
    RegSetValueExW(key, name, 0, REG_DWORD, (BYTE*)&v, sizeof(v));
  };
  write(L"CommonPhraseX", x);
  write(L"CommonPhraseY", y);
  write(L"CommonPhraseW", w);
  write(L"CommonPhraseH", h);
  RegCloseKey(key);
}

bool TrayCommonPhrasePanel::LoadGeometry(int& x, int& y, int& w, int& h) {
  HKEY key = NULL;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, KEY_READ, &key) !=
      ERROR_SUCCESS)
    return false;
  auto read = [&](const wchar_t* name, int& out, bool allow_zero) -> bool {
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    DWORD value = 0;
    if (RegQueryValueExW(key, name, NULL, &type, (BYTE*)&value, &size) !=
            ERROR_SUCCESS ||
        type != REG_DWORD)
      return false;
    if (!allow_zero && value == 0)
      return false;
    out = static_cast<int>(value);
    return true;
  };
  bool ok = read(L"CommonPhraseX", x, true) && read(L"CommonPhraseY", y, true);
  if (!read(L"CommonPhraseW", w, false))
    w = Scale(kBaseWidth);
  if (!read(L"CommonPhraseH", h, false))
    h = Scale(kBaseHeight);
  RegCloseKey(key);
  return ok;
}

int TrayCommonPhrasePanel::HitTestResize(POINT screen_pt) const {
  if (!hwnd_)
    return 0;
  RECT wr;
  GetWindowRect(hwnd_, &wr);
  const int b = Scale(kResizeBorder);
  const bool left = screen_pt.x >= wr.left && screen_pt.x < wr.left + b;
  const bool right = screen_pt.x <= wr.right && screen_pt.x > wr.right - b;
  const bool top = screen_pt.y >= wr.top && screen_pt.y < wr.top + b;
  const bool bottom = screen_pt.y <= wr.bottom && screen_pt.y > wr.bottom - b;
  if (right && bottom)
    return 17;  // HTBOTTOMRIGHT
  if (right)
    return 11;  // HTRIGHT
  if (bottom)
    return 15;  // HTBOTTOM
  if (left && bottom)
    return 16;
  if (left)
    return 10;
  if (top && right)
    return 14;
  if (top)
    return 12;
  return 0;
}

void TrayCommonPhrasePanel::PaintBackground(HDC hdc) {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  weasel_ui::PaintPanelChrome(hdc, rc, Scale(kBaseCornerRadius));
  weasel_ui::PaintTitleBar(hdc, rc, L"常用短语", font_title_, font_,
                           Scale(kBasePadding));
}

void TrayCommonPhrasePanel::OnListSelect(int index) {
  if (index < 0 || index >= static_cast<int>(phrases_.size()))
    return;
  std::wstring text = phrases_[index];
  if (on_commit_)
    on_commit_(text);
  Hide();
}

void TrayCommonPhrasePanel::OnCloseClick() {
  Hide();
}

void TrayCommonPhrasePanel::OnEditClick() {
  Hide();
  if (on_edit_)
    on_edit_();
}

void TrayCommonPhrasePanel::HandleHookKey(WPARAM vk) {
  if (!hwnd_ || !IsVisible())
    return;
  PostMessage(hwnd_, WM_KEYDOWN, vk, 0);
}

void TrayCommonPhrasePanel::StartKeyboardHook() {
  if (keyboard_hook_)
    return;
  hook_panel_ = this;
  keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, PhrasePanelKeyboardHook,
                                     GetModuleHandle(NULL), 0);
}

void TrayCommonPhrasePanel::StopKeyboardHook() {
  if (keyboard_hook_) {
    UnhookWindowsHookEx(keyboard_hook_);
    keyboard_hook_ = NULL;
  }
  if (hook_panel_ == this)
    hook_panel_ = nullptr;
}

void TrayCommonPhrasePanel::StartOutsideClickTimer() {
  if (hwnd_)
    SetTimer(hwnd_, kOutsideClickTimerId, 200, NULL);
}

void TrayCommonPhrasePanel::StopOutsideClickTimer() {
  if (hwnd_)
    KillTimer(hwnd_, kOutsideClickTimerId);
}

void TrayCommonPhrasePanel::Show(HWND parent, POINT anchor) {
  anchor = NormalizeAnchor(anchor);
  UpdateDpiScale(anchor);

  if (!hwnd_) {
    instance_ = this;
    CreateWindowClass();
    int saved_x = 0, saved_y = 0;
    int saved_w = Scale(kBaseWidth);
    int saved_h = Scale(kBaseHeight);
    LoadGeometry(saved_x, saved_y, saved_w, saved_h);
    client_w_ = saved_w;
    client_h_ = saved_h;
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        kWndClass, L"", WS_POPUP | WS_THICKFRAME, 0, 0, saved_w, saved_h,
        parent, NULL, GetModuleHandle(NULL), NULL);
    SetLayeredWindowAttributes(hwnd_, 0, kWindowAlpha, LWA_ALPHA);
    CreateControls();
  } else if (font_) {
    weasel_ui::StyleButton(btn_edit_, font_);
    if (btn_close_)
      weasel_ui::StyleButton(btn_close_, font_);
    weasel_ui::StyleEdit(list_, font_);
  }

  ReloadPhrases();
  LayoutControls();

  int x = 0, y = 0, width = client_w_, height = client_h_;
  if (LoadGeometry(x, y, width, height)) {
    client_w_ = width;
    client_h_ = height;
    LayoutControls();
  } else {
    x = anchor.x - width / 2;
    y = anchor.y - height - Scale(36);
  }

  HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  if (GetMonitorInfo(mon, &mi)) {
    if (x < mi.rcWork.left + Scale(4))
      x = mi.rcWork.left + Scale(4);
    if (x + width > mi.rcWork.right - Scale(4))
      x = mi.rcWork.right - width - Scale(4);
    if (y + height > mi.rcWork.bottom - Scale(20))
      y = mi.rcWork.bottom - height - Scale(20);
    if (y < mi.rcWork.top + Scale(4))
      y = mi.rcWork.top + Scale(4);
  }

  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  if (list_)
    SendMessage(list_, WM_SETFOCUS, 0, 0);
  InvalidateRect(hwnd_, NULL, TRUE);
  UpdateWindow(hwnd_);
  show_tick_ = GetTickCount();
  int count = static_cast<int>(SendMessage(list_, LB_GETCOUNT, 0, 0));
  if (count > 0)
    SendMessage(list_, LB_SETCURSEL, 0, 0);
  StartOutsideClickTimer();
  StartKeyboardHook();
}

void TrayCommonPhrasePanel::Hide() {
  StopOutsideClickTimer();
  StopKeyboardHook();
  if (hwnd_) {
    SaveGeometry();
    ShowWindow(hwnd_, SW_HIDE);
  }
}

bool TrayCommonPhrasePanel::IsVisible() const {
  return hwnd_ && IsWindowVisible(hwnd_);
}

LRESULT CALLBACK TrayCommonPhrasePanel::WndProc(HWND hwnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam) {
  TrayCommonPhrasePanel* self = instance_;
  switch (msg) {
    case WM_NCHITTEST: {
      if (!self)
        break;
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      const int edge = self->HitTestResize(pt);
      if (edge)
        return edge;
      ScreenToClient(hwnd, &pt);
      if (pt.y < self->Scale(kBaseHeader))
        return HTCAPTION;
      return HTCLIENT;
    }
    case WM_MOVING:
    case WM_SIZING:
      if (self)
        self->LayoutControls();
      break;
    case WM_SIZE:
      if (self) {
        self->client_w_ = LOWORD(lParam);
        self->client_h_ = HIWORD(lParam);
        self->LayoutControls();
        InvalidateRect(hwnd, NULL, TRUE);
      }
      return 0;
    case WM_LBUTTONDOWN: {
      if (!self)
        break;
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      if (pt.y < self->Scale(kBaseHeader)) {
        RECT wr;
        GetWindowRect(hwnd, &wr);
        POINT cursor;
        GetCursorPos(&cursor);
        self->dragging_ = true;
        self->drag_offset_.x = cursor.x - wr.left;
        self->drag_offset_.y = cursor.y - wr.top;
        SetCapture(hwnd);
      }
      return 0;
    }
    case WM_LBUTTONUP:
      if (self && self->dragging_) {
        self->dragging_ = false;
        ReleaseCapture();
        self->SaveGeometry();
      }
      return 0;
    case WM_MOUSEMOVE:
      if (self && self->dragging_) {
        POINT cursor;
        GetCursorPos(&cursor);
        SetWindowPos(hwnd, NULL, cursor.x - self->drag_offset_.x,
                     cursor.y - self->drag_offset_.y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    case WM_GETMINMAXINFO: {
      if (self) {
        auto* mi = reinterpret_cast<MINMAXINFO*>(lParam);
        mi->ptMinTrackSize.x = self->Scale(kMinWidth);
        mi->ptMinTrackSize.y = self->Scale(kMinHeight);
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (self) {
        switch (wParam) {
          case VK_UP: {
            int count =
                static_cast<int>(SendMessage(self->list_, LB_GETCOUNT, 0, 0));
            if (count > 0) {
              int sel = static_cast<int>(
                  SendMessage(self->list_, LB_GETCURSEL, 0, 0));
              if (sel == LB_ERR || sel <= 0)
                sel = 0;
              else
                --sel;
              SendMessage(self->list_, LB_SETCURSEL, sel, 0);
            }
            return 0;
          }
          case VK_DOWN: {
            int count =
                static_cast<int>(SendMessage(self->list_, LB_GETCOUNT, 0, 0));
            if (count > 0) {
              int sel = static_cast<int>(
                  SendMessage(self->list_, LB_GETCURSEL, 0, 0));
              if (sel == LB_ERR)
                sel = 0;
              else if (sel + 1 < count)
                ++sel;
              SendMessage(self->list_, LB_SETCURSEL, sel, 0);
            }
            return 0;
          }
          case VK_RETURN: {
            int sel = static_cast<int>(
                SendMessage(self->list_, LB_GETCURSEL, 0, 0));
            if (sel == LB_ERR)
              sel = 0;
            self->OnListSelect(sel);
            return 0;
          }
          case VK_ESCAPE:
            self->Hide();
            return 0;
        }
      }
      break;
    case WM_COMMAND:
      if (HIWORD(wParam) == BN_CLICKED && self) {
        if (LOWORD(wParam) == IDC_BTN_CLOSE)
          self->OnCloseClick();
        else if (LOWORD(wParam) == IDC_BTN_EDIT)
          self->OnEditClick();
      } else if (HIWORD(wParam) == LBN_DBLCLK && self &&
                 LOWORD(wParam) == IDC_PHRASE_LIST) {
        int sel = static_cast<int>(SendMessage(self->list_, LB_GETCURSEL, 0, 0));
        self->OnListSelect(sel);
      }
      return 0;
    case WM_TIMER:
      if (self && wParam == kOutsideClickTimerId && self->IsVisible()) {
        if (GetTickCount() - self->show_tick_ < 350)
          return 0;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
          POINT pt;
          GetCursorPos(&pt);
          HWND hit = WindowFromPoint(pt);
          if (hit != hwnd && !IsChild(hwnd, hit))
            self->Hide();
        }
      }
      return 0;
    case WM_ERASEBKGND:
      if (self) {
        self->PaintBackground((HDC)wParam);
        return 1;
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (self)
        self->PaintBackground(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DPICHANGED: {
      if (self) {
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        self->client_w_ = prc->right - prc->left;
        self->client_h_ = prc->bottom - prc->top;
        SetWindowPos(hwnd, NULL, prc->left, prc->top, self->client_w_,
                     self->client_h_, SWP_NOZORDER | SWP_NOACTIVATE);
        self->dpi_ = HIWORD(wParam);
        self->LayoutControls();
        InvalidateRect(hwnd, NULL, TRUE);
      }
      return 0;
    }
    case WM_DESTROY:
      if (self)
        self->hwnd_ = NULL;
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
