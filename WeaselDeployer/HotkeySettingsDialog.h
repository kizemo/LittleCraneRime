#pragma once



#include "resource.h"

#include "KeyCaptureEdit.h"

#include <memory>

#include <string>

#include <vector>



struct HotkeySettings {

  std::wstring toggle_ascii_l = L"Shift_L";

  std::wstring toggle_ascii_r = L"Shift_R";

  std::wstring caps_lock = L"clear";

  std::wstring pick_second = L"Shift_L";

  std::wstring pick_third = L"Shift_R";

  std::wstring page_up = L"comma";

  std::wstring page_down = L"period";

  std::wstring toggle_ascii_punct = L"Control+Shift+3";

  std::wstring toggle_full_shape = L"Control+Shift+5";

  std::wstring tab_next = L"Tab";

  std::wstring tab_prev = L"Shift+Tab";

  std::wstring alt_left = L"Alt+Left";

  std::wstring alt_right = L"Alt+Right";

  std::wstring select_first = L"bracketleft";

  std::wstring select_last = L"bracketright";

  std::wstring schema_next = L"";

  std::wstring common_phrase_panel = L"Control+Shift+K";

  std::wstring quick_bar_panel = L"Control+Shift+F";

};



class HotkeySettingsDialog : public CDialogImpl<HotkeySettingsDialog> {

 public:

  enum { IDD = IDD_HOTKEY_SETTINGS };



  BEGIN_MSG_MAP(HotkeySettingsDialog)

  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)

  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

  MESSAGE_HANDLER(WM_SIZE, OnSize)

  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)

  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)

  MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)

  COMMAND_ID_HANDLER(IDOK, OnOK)

  COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

  END_MSG_MAP()



  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnCtlColorStatic(UINT, WPARAM, LPARAM, BOOL&);

  LRESULT OnOK(WORD, WORD, HWND, BOOL&);

  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);



  static bool LoadSettings(HotkeySettings& settings);

  static bool SaveSettings(const HotkeySettings& settings);

  static bool EnsureDefaultHotkeysInstalled();



 private:

  struct RowWidgets {

    std::wstring label;

    std::wstring* value;

    bool is_group_header = false;

    CStatic name_label;

    std::unique_ptr<KeyCaptureEdit> key_edit;

    CStatic conflict_label;

  };



  void BuildUi();

  void LayoutUi(int cx, int cy);

  void ReadFromControls();

  void WriteToControls();

  void UpdateConflictLabel(RowWidgets& row);

  static std::string ExtractYamlValue(const std::string& content,

                                      const std::string& key);



  HotkeySettings settings_;

  std::vector<RowWidgets> rows_;

  CStatic header_action_;

  CStatic header_key_;

  CStatic header_hint_;

  CStatic status_label_;

  CButton btn_ok_;

  CButton btn_cancel_;

  HFONT font_normal_ = NULL;

  HFONT font_bold_ = NULL;

  HFONT font_title_ = NULL;

  HBRUSH group_brush_ = NULL;

  HBRUSH surface_brush_ = NULL;

  bool ui_built_ = false;

  int content_height_ = 0;

};

