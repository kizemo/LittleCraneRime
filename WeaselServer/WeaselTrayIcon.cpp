#include "stdafx.h"

#include "WeaselTrayIcon.h"

#include <WeaselTrayUtil.h>

#include <atlstr.h>



// nasty

#include <resource.h>



static UINT mode_icon[] = {IDI_ZH, IDI_ZH, IDI_EN, IDI_RELOAD};

static const WCHAR* mode_label[] = {NULL, /*L"中文"*/ NULL, /*L"西文"*/ NULL,

                                    L"Under maintenance"};



WeaselTrayIcon::WeaselTrayIcon(weasel::UI& ui)

    : m_style(ui.style()),

      m_status(ui.status()),

      m_mode(INITIAL),

      m_schema_zhung_icon(),

      m_schema_ascii_icon(),

      m_disabled(false) {}



void WeaselTrayIcon::CustomizeMenu(HMENU hMenu) {}



void WeaselTrayIcon::SetLeftClickHandler(std::function<void(POINT)> handler) {

  m_on_left_click = std::move(handler);

}



LRESULT WeaselTrayIcon::HandleTrayNotify(WPARAM wParam, LPARAM lParam) {
  return OnTrayNotification(wParam, lParam);
}

LRESULT WeaselTrayIcon::OnTrayNotification(WPARAM wParam, LPARAM lParam) {
  (void)wParam;
  if (LOWORD(lParam) == WM_LBUTTONUP) {
    if (m_on_left_click) {
      POINT pos;
      GetCursorPos(&pos);
      m_on_left_click(pos);
    }
    return 1;
  }
  return CSystemTray::OnTrayNotification(wParam, lParam);
}



BOOL WeaselTrayIcon::Create(HWND hTargetWnd) {

  weasel_tray::SetTrayAppIdentity();

  weasel_tray::CleanupDuplicateRunEntries();

  weasel_tray::PurgeLightTrayIcons();

  HMODULE hModule = GetModuleHandle(NULL);

  CIcon icon;

  icon.LoadIconW(IDI_ZH);



  // Register exactly one tray icon on the server window (GUID-based).

  BOOL bRet = CSystemTray::Create(

      hModule, hTargetWnd, WM_WEASEL_TRAY_NOTIFY, get_weasel_ime_name().c_str(),

      icon, weasel_tray::kWeaselTrayNotifyId, TRUE, NULL, NULL, NIIF_NONE, 10,

      IDR_MENU_POPUP, &weasel_tray::WeaselTrayNotifyGuid());



  if (hTargetWnd)

    SetTargetWnd(hTargetWnd);



  if (!m_style.display_tray_icon) {
    RemoveIcon();
  } else if (m_bRemoved && !ShowIcon()) {
    bRet = FALSE;
  }



  weasel_tray::TrayLog(L"Create", weasel_tray::kWeaselTrayNotifyId, hTargetWnd,

                       bRet);

  return bRet;

}



void WeaselTrayIcon::Refresh() {

  if (!m_style.display_tray_icon &&

      !m_status.disabled)  // display notification when deploying

  {

    if (m_mode != INITIAL) {

      RemoveIcon();

      m_mode = INITIAL;

    }

    m_disabled = false;

    return;

  }

  WeaselTrayMode mode = m_status.disabled     ? DISABLED

                        : m_status.ascii_mode ? ASCII

                                              : ZHUNG;

  if (mode != m_mode || m_schema_zhung_icon != m_style.current_zhung_icon ||

      (m_schema_zhung_icon.empty() && m_style.current_zhung_icon.empty()) ||

      m_schema_ascii_icon != m_style.current_ascii_icon ||

      (m_schema_ascii_icon.empty() && m_style.current_ascii_icon.empty())) {

    if (m_bRemoved)

      ShowIcon();

    m_mode = mode;

    m_schema_zhung_icon = m_style.current_zhung_icon;

    m_schema_ascii_icon = m_style.current_ascii_icon;

    if (mode == ASCII) {

      if (m_schema_ascii_icon.empty())

        SetIcon(mode_icon[mode]);

      else

        SetIcon(m_schema_ascii_icon.c_str());

    } else if (mode == ZHUNG) {

      if (m_schema_zhung_icon.empty())

        SetIcon(mode_icon[mode]);

      else

        SetIcon(m_schema_zhung_icon.c_str());

    } else

      SetIcon(mode_icon[mode]);



    if (mode_label[mode] && m_disabled == false) {

      CString info;

      info.LoadStringW(IDS_STR_UNDER_MAINTENANCE);

      ShowBalloon(info, get_weasel_ime_name().c_str());

      m_disabled = true;

    }

    if (m_mode != DISABLED)

      m_disabled = false;

  } else if (m_bRemoved) {

    ShowIcon();

  }

}

