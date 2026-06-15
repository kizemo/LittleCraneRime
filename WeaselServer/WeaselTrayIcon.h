#pragma once
#include <WeaselUI.h>
#include <WeaselIPC.h>
#include "SystemTraySDK.h"
#include <WeaselMessages.h>
#include <functional>

class WeaselTrayIcon : public CSystemTray {
 public:
  enum WeaselTrayMode {
    INITIAL,
    ZHUNG,
    ASCII,
    DISABLED,
  };

  WeaselTrayIcon(weasel::UI& ui);

  BOOL Create(HWND hTargetWnd);
  void Refresh();
  void SetLeftClickHandler(std::function<void(POINT)> handler);
  LRESULT HandleTrayNotify(WPARAM wParam, LPARAM lParam);

 protected:
  virtual void CustomizeMenu(HMENU hMenu);
  virtual LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);

  weasel::UIStyle& m_style;
  weasel::Status& m_status;
  WeaselTrayMode m_mode;
  std::wstring m_schema_zhung_icon;
  std::wstring m_schema_ascii_icon;
  bool m_disabled;
  std::function<void(POINT)> m_on_left_click;
};
