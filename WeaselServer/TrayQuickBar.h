#pragma once



#include <functional>

#include <string>

#include <utility>

#include <vector>



class TrayQuickBar {

 public:

  using SchemaSelectFn = std::function<void(const std::string& schema_id)>;

  using OpenHotkeyFn = std::function<void()>;

  using OpenPhraseFn = std::function<void()>;

  using OpenCommonPhraseFn = std::function<void()>;

  using GetSchemaListFn =

      std::function<std::vector<std::pair<std::string, std::string>>()>;

  using GetSchemaFn = std::function<std::string()>;



  TrayQuickBar();

  ~TrayQuickBar();



  void SetHandlers(SchemaSelectFn select_schema,

                   OpenHotkeyFn open_hotkey,

                   OpenPhraseFn open_phrase,

                   OpenCommonPhraseFn open_common_phrase,

                   GetSchemaListFn get_schema_list,

                   GetSchemaFn get_schema);



  void Show(HWND parent, POINT anchor);

  void Hide();

  bool IsVisible() const;



 private:

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,

                                  LPARAM lParam);

  void CreateWindowClass();

  void CreateControls();

  void LayoutControls();

  void OnClick(int button_id);

  void ShowSchemaMenu();

  void PaintBackground(HDC hdc);

  void StartOutsideClickTimer();

  void StopOutsideClickTimer();

  void SavePosition(int x, int y);

  bool LoadSavedPosition(int& x, int& y) const;

  bool HitDragArea(POINT client_pt) const;

  void UpdateDpiScale(POINT anchor);

  void DestroyUi();

  int Scale(int v) const;

  POINT NormalizeAnchor(POINT anchor) const;



  HWND hwnd_;

  HWND btn_schema_;

  HWND btn_hotkey_;

  HWND btn_phrase_;

  HWND btn_common_phrase_;

  HFONT font_;

  UINT dpi_;



  SchemaSelectFn select_schema_;

  OpenHotkeyFn open_hotkey_;

  OpenPhraseFn open_phrase_;

  OpenCommonPhraseFn open_common_phrase_;

  GetSchemaListFn get_schema_list_;

  GetSchemaFn get_schema_;



  DWORD show_tick_;

  bool dragging_;

  POINT drag_offset_;



  static TrayQuickBar* instance_;

  static const wchar_t* kWndClass;

  static constexpr UINT_PTR kOutsideClickTimerId = 20001;

};


