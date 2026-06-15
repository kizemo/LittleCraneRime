#pragma once

#include <functional>
#include <string>
#include <vector>

class TrayCommonPhrasePanel {
 public:
  using CommitFn = std::function<void(const std::wstring&)>;
  using EditFn = std::function<void()>;

  TrayCommonPhrasePanel();
  ~TrayCommonPhrasePanel();

  void SetHandlers(CommitFn on_commit, EditFn on_edit);
  void Show(HWND parent, POINT anchor);
  void Hide();
  bool IsVisible() const;
  void ReloadPhrases();
  void OnListSelect(int index);
  void HandleHookKey(WPARAM vk);

  static TrayCommonPhrasePanel* hook_panel_;
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  void CreateWindowClass();
  void CreateControls();
  void LayoutControls();
  void PaintBackground(HDC hdc);
  void StartOutsideClickTimer();
  void StopOutsideClickTimer();
  void UpdateDpiScale(POINT anchor);
  int Scale(int v) const;
  POINT NormalizeAnchor(POINT anchor) const;
  void OnEditClick();
  void OnCloseClick();
  void StartKeyboardHook();
  void StopKeyboardHook();
  void SaveGeometry();
  bool LoadGeometry(int& x, int& y, int& w, int& h);
  int HitTestResize(POINT screen_pt) const;

  HWND hwnd_;
  HWND list_;
  HWND btn_edit_;
  HWND btn_close_;
  HFONT font_;
  HFONT font_title_;
  UINT dpi_;
  int client_w_;
  int client_h_;
  bool dragging_;
  POINT drag_offset_;
  std::vector<std::wstring> phrases_;
  CommitFn on_commit_;
  EditFn on_edit_;
  DWORD show_tick_;
  bool suppress_sel_change_;
  HHOOK keyboard_hook_;

  static TrayCommonPhrasePanel* instance_;
  static const wchar_t* kWndClass;
  static constexpr UINT_PTR kOutsideClickTimerId = 21001;
  static constexpr int kResizeBorder = 8;
};
