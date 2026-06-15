#include "stdafx.h"
#include "TrayQuickBar.h"
#include <WeaselFileLog.h>
#include <WeaselUtility.h>
#include <WeaselUiTheme.h>
#include <ShellScalingApi.h>

#pragma comment(lib, "shcore.lib")

const wchar_t* TrayQuickBar::kWndClass = L"WeaselTrayQuickBar";
TrayQuickBar* TrayQuickBar::instance_ = nullptr;

#define IDC_BTN_SCHEMA  2001
#define IDC_BTN_HOTKEY  2002
#define IDC_BTN_PHRASE  2003
#define IDC_BTN_COMMON  2004
#define IDC_SCHEMA_BASE 3001

namespace {
constexpr int kBaseBarHeight = 40;
constexpr int kBaseBtnWidth = 72;
constexpr int kBaseBtnHeight = 28;
constexpr int kBasePadding = 8;
constexpr int kBaseGap = 6;
constexpr int kBtnCount = 4;
constexpr int kBaseBtnWidthCommon = 64;
constexpr int kBaseCornerRadius = 12;
constexpr BYTE kWindowAlpha = 235;
constexpr COLORREF kBgColor = weasel_ui::kSurface;
constexpr COLORREF kBtnColor = weasel_ui::kSurfaceElevated;
constexpr COLORREF kBtnHover = weasel_ui::kAccentSoft;
constexpr COLORREF kTextColor = weasel_ui::kTextPrimary;
constexpr COLORREF kBorderColor = weasel_ui::kBorder;

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

void DrawButton(HDC hdc, RECT rc, const wchar_t* text, bool hover, HFONT font) {
  weasel_ui::DrawThemedButton(hdc, rc, text, hover, false, font, 8);
}
}  // namespace

TrayQuickBar::TrayQuickBar()
    : hwnd_(NULL),
      btn_schema_(NULL),
      btn_hotkey_(NULL),
      btn_phrase_(NULL),
      btn_common_phrase_(NULL),
      font_(NULL),
      dpi_(96),
      show_tick_(0),
      dragging_(false) {}

TrayQuickBar::~TrayQuickBar() {
  DestroyUi();
  if (font_) {
    DeleteObject(font_);
    font_ = NULL;
  }
}

void TrayQuickBar::DestroyUi() {
  StopOutsideClickTimer();
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
  btn_schema_ = btn_hotkey_ = btn_phrase_ = btn_common_phrase_ = NULL;
}

void TrayQuickBar::SetHandlers(SchemaSelectFn select_schema,
                               OpenHotkeyFn open_hotkey,
                               OpenPhraseFn open_phrase,
                               OpenCommonPhraseFn open_common_phrase,
                               GetSchemaListFn get_schema_list,
                               GetSchemaFn get_schema) {
  select_schema_ = std::move(select_schema);
  open_hotkey_ = std::move(open_hotkey);
  open_phrase_ = std::move(open_phrase);
  open_common_phrase_ = std::move(open_common_phrase);
  get_schema_list_ = std::move(get_schema_list);
  get_schema_ = std::move(get_schema);
}

int TrayQuickBar::Scale(int v) const {
  return MulDiv(v, dpi_, 96);
}

void TrayQuickBar::UpdateDpiScale(POINT anchor) {
  dpi_ = GetDpiForPoint(anchor);
  if (font_) {
    DeleteObject(font_);
    font_ = NULL;
  }
  int font_height = -MulDiv(13, dpi_, 96);
  font_ = CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                      L"Microsoft YaHei UI");
}

POINT TrayQuickBar::NormalizeAnchor(POINT anchor) const {
  GetCursorPos(&anchor);

  using PTLPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
  auto PhysicalToLogical =
      (PTLPMDPI)GetProcAddress(GetModuleHandle(L"user32.dll"),
                               "PhysicalToLogicalPointForPerMonitorDPI");
  if (PhysicalToLogical)
    PhysicalToLogical(NULL, &anchor);
  return anchor;
}

void TrayQuickBar::CreateWindowClass() {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = kWndClass;
  wc.hbrBackground = NULL;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  RegisterClassExW(&wc);
}

void TrayQuickBar::CreateControls() {
  int pad = Scale(kBasePadding);
  int btn_w = Scale(kBaseBtnWidth);
  int btn_h = Scale(kBaseBtnHeight);

  auto make_btn = [&](int id, const wchar_t* text, int x) -> HWND {
    HWND btn = CreateWindowW(
        L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, pad, btn_w,
        btn_h, hwnd_, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    if (font_)
      SendMessage(btn, WM_SETFONT, (WPARAM)font_, TRUE);
    return btn;
  };

  int x = pad;
  btn_schema_ = make_btn(IDC_BTN_SCHEMA, L"方案", x);
  x += btn_w + Scale(kBaseGap);
  btn_hotkey_ = make_btn(IDC_BTN_HOTKEY, L"快捷键", x);
  x += btn_w + Scale(kBaseGap);
  btn_phrase_ = make_btn(IDC_BTN_PHRASE, L"词典", x);
  x += btn_w + Scale(kBaseGap);
  int common_w = Scale(kBaseBtnWidthCommon);
  btn_common_phrase_ = CreateWindowW(
      L"BUTTON", L"常用", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, pad,
      common_w, btn_h, hwnd_, (HMENU)(INT_PTR)IDC_BTN_COMMON,
      GetModuleHandle(NULL), NULL);
  if (font_)
    SendMessage(btn_common_phrase_, WM_SETFONT, (WPARAM)font_, TRUE);
}

void TrayQuickBar::LayoutControls() {
  int pad = Scale(kBasePadding);
  int btn_w = Scale(kBaseBtnWidth);
  int gap = Scale(kBaseGap);
  int common_w = Scale(kBaseBtnWidthCommon);
  int width = pad * 2 + btn_w * (kBtnCount - 1) + common_w + gap * (kBtnCount - 1);
  int height = Scale(kBaseBarHeight);
  SetWindowPos(hwnd_, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

  int x = pad;
  int btn_h = Scale(kBaseBtnHeight);
  SetWindowPos(btn_schema_, NULL, x, pad, btn_w, btn_h, SWP_NOZORDER);
  x += btn_w + gap;
  SetWindowPos(btn_hotkey_, NULL, x, pad, btn_w, btn_h, SWP_NOZORDER);
  x += btn_w + gap;
  SetWindowPos(btn_phrase_, NULL, x, pad, btn_w, btn_h, SWP_NOZORDER);
  x += btn_w + gap;
  SetWindowPos(btn_common_phrase_, NULL, x, pad, common_w, btn_h, SWP_NOZORDER);

  int radius = Scale(kBaseCornerRadius);
  HRGN rgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
  SetWindowRgn(hwnd_, rgn, TRUE);
}

void TrayQuickBar::PaintBackground(HDC hdc) {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  weasel_ui::PaintPanelChrome(hdc, rc, Scale(kBaseCornerRadius));
  HPEN drag_pen = CreatePen(PS_SOLID, Scale(2), weasel_ui::kBorder);
  HPEN old_pen = (HPEN)SelectObject(hdc, drag_pen);
  int drag_y = Scale(3);
  MoveToEx(hdc, Scale(12), drag_y, NULL);
  LineTo(hdc, rc.right - Scale(12), drag_y);
  SelectObject(hdc, old_pen);
  DeleteObject(drag_pen);
}

void TrayQuickBar::ShowSchemaMenu() {
  if (!get_schema_list_ || !hwnd_)
    return;

  auto schemas = get_schema_list_();
  if (schemas.empty())
    return;

  HMENU menu = CreatePopupMenu();
  std::string current = get_schema_ ? get_schema_() : "";
  for (size_t i = 0; i < schemas.size(); ++i) {
    UINT flags = MF_STRING;
    if (schemas[i].first == current)
      flags |= MF_CHECKED;
    std::wstring label = u8tow(schemas[i].second);
    AppendMenuW(menu, flags, IDC_SCHEMA_BASE + static_cast<UINT>(i),
                label.c_str());
  }

  RECT btn_rc;
  GetWindowRect(btn_schema_, &btn_rc);
  UINT cmd = TrackPopupMenu(
      menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, btn_rc.left,
      btn_rc.bottom + Scale(4), 0, hwnd_, NULL);
  DestroyMenu(menu);

  if (cmd >= IDC_SCHEMA_BASE && select_schema_) {
    size_t idx = cmd - IDC_SCHEMA_BASE;
    if (idx < schemas.size())
      select_schema_(schemas[idx].first);
  }
}

void TrayQuickBar::OnClick(int button_id) {
  switch (button_id) {
    case IDC_BTN_SCHEMA:
      ShowSchemaMenu();
      break;
    case IDC_BTN_HOTKEY:
      Hide();
      if (open_hotkey_)
        open_hotkey_();
      return;
    case IDC_BTN_PHRASE:
      Hide();
      if (open_phrase_)
        open_phrase_();
      return;
    case IDC_BTN_COMMON:
      Hide();
      if (open_common_phrase_)
        open_common_phrase_();
      return;
  }
}

void TrayQuickBar::SavePosition(int x, int y) {
  HKEY key = NULL;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel\\UI", 0, NULL,
                      0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
    RegSetValueExW(key, L"QuickBarX", 0, REG_DWORD, (BYTE*)&x, sizeof(x));
    RegSetValueExW(key, L"QuickBarY", 0, REG_DWORD, (BYTE*)&y, sizeof(y));
    RegCloseKey(key);
  }
}

bool TrayQuickBar::LoadSavedPosition(int& x, int& y) const {
  HKEY key = NULL;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel\\UI", 0,
                    KEY_READ, &key) != ERROR_SUCCESS)
    return false;
  DWORD type = REG_DWORD;
  DWORD size = sizeof(DWORD);
  DWORD dx = static_cast<DWORD>(-1);
  DWORD dy = static_cast<DWORD>(-1);
  bool ok = RegQueryValueExW(key, L"QuickBarX", NULL, &type, (BYTE*)&dx,
                             &size) == ERROR_SUCCESS &&
            RegQueryValueExW(key, L"QuickBarY", NULL, &type, (BYTE*)&dy,
                             &size) == ERROR_SUCCESS &&
            dx != static_cast<DWORD>(-1) && dy != static_cast<DWORD>(-1);
  RegCloseKey(key);
  if (!ok)
    return false;
  x = static_cast<int>(dx);
  y = static_cast<int>(dy);
  return true;
}

bool TrayQuickBar::HitDragArea(POINT client_pt) const {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  RECT drag_rc = rc;
  drag_rc.bottom = Scale(6);
  return PtInRect(&drag_rc, client_pt) != 0;
}

void TrayQuickBar::StartOutsideClickTimer() {
  if (hwnd_)
    SetTimer(hwnd_, kOutsideClickTimerId, 350, NULL);
}

void TrayQuickBar::StopOutsideClickTimer() {
  if (hwnd_)
    KillTimer(hwnd_, kOutsideClickTimerId);
}

void TrayQuickBar::Show(HWND parent, POINT anchor) {
  anchor = NormalizeAnchor(anchor);
  UpdateDpiScale(anchor);

  if (hwnd_ && (!btn_common_phrase_ || !IsWindow(btn_common_phrase_)))
    DestroyUi();

  if (!hwnd_) {
    instance_ = this;
    CreateWindowClass();
    int pad = Scale(kBasePadding);
    int btn_w = Scale(kBaseBtnWidth);
    int gap = Scale(kBaseGap);
    int common_w = Scale(kBaseBtnWidthCommon);
  int width = pad * 2 + btn_w * (kBtnCount - 1) + common_w + gap * (kBtnCount - 1);
    int height = Scale(kBaseBarHeight);
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        kWndClass, L"", WS_POPUP, 0, 0, width, height, parent, NULL,
        GetModuleHandle(NULL), NULL);
    SetLayeredWindowAttributes(hwnd_, 0, kWindowAlpha, LWA_ALPHA);
    CreateControls();
  } else if (font_) {
    SendMessage(btn_schema_, WM_SETFONT, (WPARAM)font_, TRUE);
    SendMessage(btn_hotkey_, WM_SETFONT, (WPARAM)font_, TRUE);
    SendMessage(btn_phrase_, WM_SETFONT, (WPARAM)font_, TRUE);
    SendMessage(btn_common_phrase_, WM_SETFONT, (WPARAM)font_, TRUE);
  }

  LayoutControls();

  RECT rc;
  GetWindowRect(hwnd_, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;
  int x = 0;
  int y = 0;
  if (!LoadSavedPosition(x, y)) {
    x = anchor.x - width / 2;
    y = anchor.y - height - Scale(36);
    HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfo(mon, &mi)) {
      if (x < mi.rcWork.left + Scale(4))
        x = mi.rcWork.left + Scale(4);
      if (x + width > mi.rcWork.right - Scale(4))
        x = mi.rcWork.right - width - Scale(4);
      if (y + height > mi.rcWork.bottom - Scale(24))
        y = mi.rcWork.bottom - height - Scale(24);
      if (y < mi.rcWork.top + Scale(4))
        y = mi.rcWork.top + Scale(4);
    }
  }

  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  InvalidateRect(hwnd_, NULL, TRUE);
  show_tick_ = GetTickCount();
  StartOutsideClickTimer();
}

void TrayQuickBar::Hide() {
  StopOutsideClickTimer();
  if (hwnd_)
    ShowWindow(hwnd_, SW_HIDE);
}

bool TrayQuickBar::IsVisible() const {
  return hwnd_ && IsWindowVisible(hwnd_);
}

LRESULT CALLBACK TrayQuickBar::WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
  TrayQuickBar* self = instance_;
  switch (msg) {
    case WM_COMMAND:
      if (HIWORD(wParam) == BN_CLICKED && self)
        self->OnClick(LOWORD(wParam));
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
    case WM_DRAWITEM: {
      LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
      if (dis->CtlType == ODT_BUTTON && self) {
        wchar_t text[64] = {0};
        GetWindowTextW(dis->hwndItem, text, 64);
        bool hover = (dis->itemState & ODS_SELECTED) != 0;
        DrawButton(dis->hDC, dis->rcItem, text, hover, self->font_);
        return TRUE;
      }
      break;
    }
    case WM_LBUTTONDOWN:
      if (self) {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (self->HitDragArea(pt)) {
          self->dragging_ = true;
          RECT wr;
          GetWindowRect(hwnd, &wr);
          POINT cursor;
          GetCursorPos(&cursor);
          self->drag_offset_.x = cursor.x - wr.left;
          self->drag_offset_.y = cursor.y - wr.top;
          SetCapture(hwnd);
          return 0;
        }
      }
      break;
    case WM_MOUSEMOVE:
      if (self && self->dragging_) {
        POINT cursor;
        GetCursorPos(&cursor);
        int nx = cursor.x - self->drag_offset_.x;
        int ny = cursor.y - self->drag_offset_.y;
        SetWindowPos(hwnd, HWND_TOPMOST, nx, ny, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
      }
      break;
    case WM_LBUTTONUP:
      if (self && self->dragging_) {
        self->dragging_ = false;
        ReleaseCapture();
        RECT wr;
        GetWindowRect(hwnd, &wr);
        self->SavePosition(wr.left, wr.top);
        return 0;
      }
      break;
    case WM_TIMER:
      if (self && wParam == kOutsideClickTimerId) {
        if (GetTickCount() - self->show_tick_ < 400)
          return 0;
        if (self->dragging_)
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
    case WM_DPICHANGED: {
      if (self) {
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left,
                     prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
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
