#pragma once

#include <windows.h>

#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

// Calm B2B tool UI — Microsoft YaHei UI, low density, candidate-bar inspired chrome.

namespace weasel_ui {

constexpr COLORREF kSurface = RGB(248, 249, 251);
constexpr COLORREF kSurfaceElevated = RGB(255, 255, 255);
constexpr COLORREF kSurfaceHover = RGB(239, 246, 255);
constexpr COLORREF kBorder = RGB(203, 213, 225);
constexpr COLORREF kBorderFocus = RGB(59, 130, 246);
constexpr COLORREF kTextPrimary = RGB(15, 23, 42);
constexpr COLORREF kTextSecondary = RGB(100, 116, 139);
constexpr COLORREF kAccentSoft = RGB(219, 234, 254);
constexpr COLORREF kAccent = RGB(37, 99, 235);
constexpr COLORREF kAccentText = RGB(255, 255, 255);
constexpr COLORREF kGroupBg = RGB(241, 245, 249);
constexpr COLORREF kListAlt = RGB(252, 253, 255);
constexpr COLORREF kShadow = RGB(148, 163, 184);
constexpr BYTE kPanelAlpha = 252;

// Dark title bar — matches candidate bar palette.
constexpr COLORREF kTitleBarBg = RGB(28, 28, 30);
constexpr COLORREF kTitleBarText = RGB(255, 255, 255);
constexpr COLORREF kTitleBarAccent = RGB(204, 224, 128);
constexpr COLORREF kTitleBarBorder = RGB(48, 48, 52);
constexpr int kTitleBarHeightDp = 44;

inline HFONT CreateThemeFont(int point_size, UINT dpi, bool semibold = false) {
  int height = -MulDiv(point_size, dpi, 72);
  return CreateFontW(height, 0, 0, 0, semibold ? FW_SEMIBOLD : FW_NORMAL, FALSE,
                     FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

inline void StyleButton(HWND hwnd, HFONT font) {
  if (font)
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

inline void StyleEdit(HWND hwnd, HFONT font) {
  if (font)
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

inline void FillRoundRect(HDC hdc, const RECT& rc, COLORREF fill, COLORREF border,
                          int radius) {
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(hdc, &rc, brush);
  DeleteObject(brush);
  HPEN pen = CreatePen(PS_SOLID, 1, border);
  HPEN old_pen = (HPEN)SelectObject(hdc, pen);
  HBRUSH null_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
  HBRUSH old_brush = (HBRUSH)SelectObject(hdc, null_brush);
  RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
}

inline void PaintPanelChrome(HDC hdc, const RECT& rc, int radius) {
  FillRoundRect(hdc, rc, kTitleBarBg, kTitleBarBorder, radius);
}

inline void PaintTitleBar(HDC hdc, const RECT& rc, const wchar_t* title,
                          HFONT font_title, HFONT font_sub, int pad) {
  RECT bar = rc;
  bar.bottom = bar.top + pad + 28;
  HBRUSH bar_brush = CreateSolidBrush(kTitleBarBg);
  FillRect(hdc, &bar, bar_brush);
  DeleteObject(bar_brush);

  RECT accent = {bar.left, bar.top, bar.left + 3, bar.bottom};
  HBRUSH accent_brush = CreateSolidBrush(kTitleBarAccent);
  FillRect(hdc, &accent, accent_brush);
  DeleteObject(accent_brush);

  SetBkMode(hdc, TRANSPARENT);
  HFONT old = (HFONT)SelectObject(hdc, font_title ? font_title : font_sub);
  SetTextColor(hdc, kTitleBarText);
  RECT title_rc = {pad + 6, bar.top + 6, rc.right - pad, bar.bottom - 4};
  DrawTextW(hdc, title, -1, &title_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  SelectObject(hdc, old);

  HPEN sep = CreatePen(PS_SOLID, 1, kTitleBarBorder);
  HPEN op = (HPEN)SelectObject(hdc, sep);
  MoveToEx(hdc, rc.left, bar.bottom - 1, NULL);
  LineTo(hdc, rc.right, bar.bottom - 1);
  SelectObject(hdc, op);
  DeleteObject(sep);
}

inline void DrawThemedButton(HDC hdc, const RECT& rc, const wchar_t* text,
                             bool hover, bool pressed, HFONT font, int radius) {
  COLORREF bg = pressed ? kAccent : (hover ? kSurfaceHover : kSurfaceElevated);
  COLORREF border = hover ? kBorderFocus : kBorder;
  COLORREF text_color = pressed ? kAccentText : kTextPrimary;
  FillRoundRect(hdc, rc, bg, border, radius);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, text_color);
  HFONT old = (HFONT)SelectObject(hdc, font);
  DrawTextW(hdc, text, -1, const_cast<RECT*>(&rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  SelectObject(hdc, old);
}

inline void ApplyDialogChrome(HWND hwnd) {
  SetWindowTheme(hwnd, L"Explorer", NULL);
}

}  // namespace weasel_ui
