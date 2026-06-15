#include "stdafx.h"
#include "WeaselServerApp.h"
#include <WeaselConstants.h>
#include <WeaselFileLog.h>
#include <WeaselIPC.h>
#include <WeaselTrayUtil.h>
#include <filesystem>

WeaselServerApp::WeaselServerApp()
    : m_handler(std::make_unique<RimeWithWeaselHandler>(&m_ui)),
      tray_icon(m_ui) {
  // m_handler.reset(new RimeWithWeaselHandler(&m_ui));
  m_server.SetRequestHandler(m_handler.get());
  SetupMenuHandlers();
}

WeaselServerApp::~WeaselServerApp() {}

int WeaselServerApp::Run() {
  weasel_tray::CleanupDuplicateRunEntries();
  weasel_tray::PurgeLightTrayIcons();
  WeaselAppendLogW(L"weasel.server.log", L"startup",
                   L"WeaselServer " WEASEL_VERSION L" starting");
  if (!m_server.Start())
    return -1;

  {
    std::string feed_url = GetCustomResource("FeedURL", "APPCAST");
    if (!feed_url.empty())
      win_sparkle_set_appcast_url(feed_url.c_str());
  }
  win_sparkle_set_registry_path("Software\\Rime\\Weasel\\Updates");
  if (GetThreadUILanguage() ==
      MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL))
    win_sparkle_set_lang("zh-TW");
  else if (GetThreadUILanguage() ==
           MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
    win_sparkle_set_lang("zh-CN");
  else
    win_sparkle_set_lang("en");
  win_sparkle_init();
  m_ui.Create(m_server.GetHWnd());

  m_handler->Initialize();
  m_handler->OnUpdateUI([this]() { tray_icon.Refresh(); });

  m_server.SetTrayNotifyHandler([this](WPARAM wParam, LPARAM lParam) {
    return tray_icon.HandleTrayNotify(wParam, lParam);
  });
  tray_icon.Create(m_server.GetHWnd());
  tray_icon.Refresh();

  m_server.SetQuickBarHandler([this](POINT pos) {
    if (m_quick_bar.IsVisible())
      m_quick_bar.Hide();
    else
      m_quick_bar.Show(m_server.GetHWnd(), pos);
  });

  auto show_common_phrases = [this]() {
    POINT pos;
    GetCursorPos(&pos);
    if (m_common_phrase_panel.IsVisible())
      m_common_phrase_panel.Hide();
    else {
      m_common_phrase_panel.ReloadPhrases();
      m_common_phrase_panel.Show(m_server.GetHWnd(), pos);
    }
  };

  m_server.SetCommonPhraseHandler(show_common_phrases);

  m_common_phrase_panel.SetHandlers(
      [this](const std::wstring& text) { m_handler->RequestCommitText(text); },
      [this]() {
        std::filesystem::path dir = install_dir();
        execute(dir / L"WeaselDeployer.exe", std::wstring(L"/commonphrase"));
      });

  m_quick_bar.SetHandlers(
      [this](const std::string& schema_id) {
        m_handler->SelectSchema(schema_id);
      },
      [this]() {
        std::filesystem::path dir = install_dir();
        execute(dir / L"WeaselDeployer.exe", std::wstring(L"/hotkey"));
      },
      [this]() {
        std::filesystem::path dir = install_dir();
        execute(dir / L"WeaselDeployer.exe", std::wstring(L"/phrase"));
      },
      show_common_phrases,
      [this]() { return m_handler->GetSchemaList(); },
      [this]() { return m_handler->GetCurrentSchemaId(); });

  tray_icon.SetLeftClickHandler([this](POINT pos) {
    if (m_quick_bar.IsVisible())
      m_quick_bar.Hide();
    else
      m_quick_bar.Show(m_server.GetHWnd(), pos);
  });

  int ret = m_server.Run();

  m_handler->Finalize();
  m_ui.Destroy();
  tray_icon.RemoveIcon();
  weasel_tray::PurgeAllLegacyWeaselTrayIcons();
  win_sparkle_cleanup();

  return ret;
}

void WeaselServerApp::SetupMenuHandlers() {
  std::filesystem::path dir = install_dir();
  m_server.AddMenuHandler(ID_WEASELTRAY_QUIT,
                          [this] { return m_server.Stop() == 0; });
  m_server.AddMenuHandler(ID_WEASELTRAY_DEPLOY,
                          std::bind(execute, dir / L"WeaselDeployer.exe",
                                    std::wstring(L"/deploy")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SETTINGS,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring()));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_DICT_MANAGEMENT,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/dict")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SYNC,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/sync")));
  m_server.AddMenuHandler(ID_WEASELTRAY_WIKI,
                          std::bind(open, L"https://rime.im/docs/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_HOMEPAGE,
                          std::bind(open, L"https://rime.im/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_FORUM,
                          std::bind(open, L"https://rime.im/discuss/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_CHECKUPDATE, check_update);
  m_server.AddMenuHandler(ID_WEASELTRAY_INSTALLDIR, std::bind(explore, dir));
  m_server.AddMenuHandler(ID_WEASELTRAY_USERCONFIG,
                          std::bind(explore, WeaselUserDataPath()));
  m_server.AddMenuHandler(ID_WEASELTRAY_LOGDIR,
                          std::bind(explore, WeaselLogPath()));
  m_server.AddMenuHandler(ID_WEASELTRAY_COMMON_PHRASES, [this]() {
    POINT pos;
    GetCursorPos(&pos);
    if (m_common_phrase_panel.IsVisible())
      m_common_phrase_panel.Hide();
    else {
      m_common_phrase_panel.ReloadPhrases();
      m_common_phrase_panel.Show(m_server.GetHWnd(), pos);
    }
    return true;
  });
  m_server.AddMenuHandler(ID_WEASELTRAY_QUICK_BAR, [this]() {
    POINT pos = m_server.TakeQuickBarAnchor();
    if (pos.x < 0)
      GetCursorPos(&pos);
    if (m_quick_bar.IsVisible())
      m_quick_bar.Hide();
    else
      m_quick_bar.Show(m_server.GetHWnd(), pos);
    return true;
  });
}
