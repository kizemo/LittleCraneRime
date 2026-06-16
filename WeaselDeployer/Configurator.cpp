#include "stdafx.h"
#include "WeaselDeployer.h"
#include "Configurator.h"
#include "SwitcherSettingsDialog.h"
#include "UIStyleSettings.h"
#include "UIStyleSettingsDialog.h"
#include "DictManagementDialog.h"
#include "CustomPhraseDialog.h"
#include "CustomPhraseListDialog.h"
#include "HotkeySettingsDialog.h"
#include "CommonPhraseEditDialog.h"
#include <WeaselConstants.h>
#include <WeaselIPC.h>
#include <WeaselIPCData.h>
#include <WeaselUtility.h>
#pragma warning(disable : 4005)
#include <rime_api.h>
#include <rime_levers_api.h>
#pragma warning(default : 4005)
#include <filesystem>
#include <fstream>
#include "WeaselDeployer.h"

static std::wstring ReadTextFile(const std::filesystem::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs)
    return {};
  std::string bytes((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
  return u8tow(bytes);
}

static bool WriteTextFile(const std::filesystem::path& path,
                          const std::wstring& text) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs)
    return false;
  std::string bytes = wtou8(text);
  ofs.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return !!ofs;
}

// Keep Shift noop so TSF _ToggleAsciiMode handles single-click toggle.
// patch-rime-ice-config.ps1 and shared default.yaml already use noop;
// weasel_hotkeys + an older deploy hook used to force commit_code and break redeploy.
static void EnsureDeployAsciiConfig() {
  namespace fs = std::filesystem;
  const fs::path user_dir = WeaselUserDataPath();

  const auto replace_all = [](std::wstring& content, const wchar_t* from,
                              const wchar_t* to) -> bool {
    bool changed = false;
    size_t pos = 0;
    const size_t from_len = wcslen(from);
    const size_t to_len = wcslen(to);
    while ((pos = content.find(from, pos)) != std::wstring::npos) {
      content.replace(pos, from_len, to);
      pos += to_len;
      changed = true;
    }
    return changed;
  };

  const fs::path hotkeys = user_dir / L"weasel_hotkeys.yaml";
  if (fs::exists(hotkeys)) {
    std::wstring hk = ReadTextFile(hotkeys);
    bool changed = false;
    changed |= replace_all(hk, L"ascii_composer/switch_key/Shift_L: commit_code",
                           L"ascii_composer/switch_key/Shift_L: noop");
    changed |= replace_all(hk, L"ascii_composer/switch_key/Shift_R: commit_code",
                           L"ascii_composer/switch_key/Shift_R: noop");
    if (hk.find(L"Control+space") == std::wstring::npos &&
        hk.find(L"Control+Space") == std::wstring::npos) {
      if (hk.find(L"patch:") != std::wstring::npos) {
        const size_t pos = hk.find(L"patch:");
        const size_t line_end = hk.find(L'\n', pos);
        hk.insert(line_end == std::wstring::npos ? hk.size() : line_end + 1,
                  L"  key_binder/bindings/+:\n"
                  L"    - { when: always, toggle: ascii_mode, accept: "
                  L"Control+space }\n");
        changed = true;
      }
    }
    if (changed)
      WriteTextFile(hotkeys, hk);
  }

  const fs::path user_default = user_dir / L"default.yaml";
  if (fs::exists(user_default)) {
    std::wstring content = ReadTextFile(user_default);
    if (!content.empty()) {
      bool changed = false;
      changed |= replace_all(content, L"ascii_composer/switch_key/Shift_L: commit_code",
                           L"ascii_composer/switch_key/Shift_L: noop");
      changed |= replace_all(content, L"ascii_composer/switch_key/Shift_R: commit_code",
                           L"ascii_composer/switch_key/Shift_R: noop");
      if (changed)
        WriteTextFile(user_default, content);
    }
  }
}

static void CreateFileIfNotExist(std::string filename) {
  std::filesystem::path file_path = WeaselUserDataPath() / u8tow(filename);
  DWORD dwAttrib = GetFileAttributes(file_path.c_str());
  if (!(INVALID_FILE_ATTRIBUTES != dwAttrib &&
        0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
    std::wofstream o(file_path.c_str(), std::ios::app);
    o.close();
  }
}
Configurator::Configurator() {
  CreateFileIfNotExist("default.custom.yaml");
  CreateFileIfNotExist("weasel.custom.yaml");
  HotkeySettingsDialog::EnsureDefaultHotkeysInstalled();
}

void Configurator::Initialize() {
  RIME_STRUCT(RimeTraits, weasel_traits);
  std::string shared_dir = wtou8(WeaselSharedDataPath().wstring());
  std::string user_dir = wtou8(WeaselUserDataPath().wstring());
  weasel_traits.shared_data_dir = shared_dir.c_str();
  weasel_traits.user_data_dir = user_dir.c_str();
  weasel_traits.prebuilt_data_dir = weasel_traits.shared_data_dir;
  std::string distribution_name = wtou8(get_weasel_ime_name());
  weasel_traits.distribution_name = distribution_name.c_str();
  weasel_traits.distribution_code_name = WEASEL_CODE_NAME;
  weasel_traits.distribution_version = WEASEL_VERSION;
  weasel_traits.app_name = "rime.weasel";
  std::string log_dir = WeaselLogPath().u8string();
  weasel_traits.log_dir = log_dir.c_str();
  RimeApi* rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&weasel_traits);
  LOG(INFO) << "WeaselDeployer reporting.";
  rime_api->deployer_initialize(NULL);
}

static bool configure_switcher(RimeLeversApi* api,
                               RimeSwitcherSettings* switchcer_settings,
                               bool* reconfigured) {
  RimeCustomSettings* settings = (RimeCustomSettings*)switchcer_settings;
  if (!api->load_settings(settings))
    return false;
  SwitcherSettingsDialog dialog(switchcer_settings);
  if (dialog.DoModal() == IDOK) {
    if (api->save_settings(settings))
      *reconfigured = true;
    return true;
  }
  return false;
}

static bool configure_ui(RimeLeversApi* api,
                         UIStyleSettings* ui_style_settings,
                         bool* reconfigured) {
  RimeCustomSettings* settings = ui_style_settings->settings();
  if (!api->load_settings(settings))
    return false;
  UIStyleSettingsDialog dialog(ui_style_settings);
  if (dialog.DoModal() == IDOK) {
    if (api->save_settings(settings))
      *reconfigured = true;
    return true;
  }
  return false;
}

int Configurator::Run(bool installing) {
  RimeModule* levers = rime_get_api()->find_module("levers");
  if (!levers)
    return 1;
  RimeLeversApi* api = (RimeLeversApi*)levers->get_api();
  if (!api)
    return 1;

  bool reconfigured = false;

  RimeSwitcherSettings* switcher_settings = api->switcher_settings_init();
  UIStyleSettings ui_style_settings;

  bool is_first_run = api->is_first_run((RimeCustomSettings*)switcher_settings);
  bool skip_switcher_settings = installing && !is_first_run;
  bool skip_ui_style_settings = installing && !is_first_run;

  (skip_switcher_settings ||
   configure_switcher(api, switcher_settings, &reconfigured)) &&
      (skip_ui_style_settings ||
       configure_ui(api, &ui_style_settings, &reconfigured));

  api->custom_settings_destroy((RimeCustomSettings*)switcher_settings);

  if (installing || reconfigured) {
    return UpdateWorkspace(reconfigured);
  }
  return 0;
}

int Configurator::UpdateWorkspace(bool report_errors) {
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
  if (!hMutex) {
    LOG(ERROR) << "Error creating WeaselDeployerMutex.";
    return 1;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    LOG(WARNING) << "another deployer process is running; aborting operation.";
    CloseHandle(hMutex);
    if (report_errors) {
      // MessageBox(NULL,
      // L"正在執行另一項部署任務，方纔所做的修改將在輸入法再次啓動後生效。",
      // L"【小鹤】", MB_OK | MB_ICONINFORMATION);
      MSG_BY_IDS(IDS_STR_DEPLOYING_RESTARTREQ, IDS_STR_WEASEL,
                 MB_OK | MB_ICONINFORMATION);
    }
    return 1;
  }

  weasel::Client client;
  if (client.Connect()) {
    LOG(INFO) << "Turning WeaselServer into maintenance mode.";
    client.StartMaintenance();
  }

  {
    RimeApi* rime = rime_get_api();
    HotkeySettingsDialog::EnsureDefaultHotkeysInstalled();
    EnsureDeployAsciiConfig();
    // initialize default config, preset schemas
    rime->deploy();
    // initialize weasel config
    rime->deploy_config_file("weasel.yaml", "config_version");
    // Re-apply after deploy in case workspace_update synced stale yaml.
    EnsureDeployAsciiConfig();
    rime->deploy();
  }

  CloseHandle(hMutex);  // should be closed before resuming service.

  if (client.Connect()) {
    LOG(INFO) << "Resuming service.";
    client.EndMaintenance();
  }
  return 0;
}

int Configurator::DictManagement() {
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
  if (!hMutex) {
    LOG(ERROR) << "Error creating WeaselDeployerMutex.";
    return 1;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    LOG(WARNING) << "another deployer process is running; aborting operation.";
    CloseHandle(hMutex);
    // MessageBox(NULL, L"正在執行另一項部署任務，請稍候再試。", L"【小鹤】",
    // MB_OK | MB_ICONINFORMATION);
    MSG_BY_IDS(IDS_STR_DEPLOYING_WAIT, IDS_STR_WEASEL,
               MB_OK | MB_ICONINFORMATION);
    return 1;
  }

  weasel::Client client;
  if (client.Connect()) {
    LOG(INFO) << "Turning WeaselServer into maintenance mode.";
    client.StartMaintenance();
  }

  {
    RimeApi* rime = rime_get_api();
    if (RIME_API_AVAILABLE(rime, run_task)) {
      rime->run_task("installation_update");  // setup user data sync dir
    }
    DictManagementDialog dlg;
    dlg.DoModal();
  }

  CloseHandle(hMutex);  // should be closed before resuming service.

  if (client.Connect()) {
    LOG(INFO) << "Resuming service.";
    client.EndMaintenance();
  }
  return 0;
}

int Configurator::CustomPhraseEntry() {
  weasel::Client client;
  if (client.Connect())
    client.StartMaintenance();
  CustomPhraseListDialog dlg;
  dlg.DoModal();
  if (client.Connect())
    client.EndMaintenance();
  return 0;
}

int Configurator::CommonPhraseEdit() {
  CommonPhraseEditDialog dlg;
  dlg.DoModal();
  return 0;
}

int Configurator::HotkeySettings() {
  HotkeySettingsDialog dlg;
  dlg.DoModal();
  return 0;
}

int Configurator::SyncUserData() {
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
  if (!hMutex) {
    LOG(ERROR) << "Error creating WeaselDeployerMutex.";
    return 1;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    LOG(WARNING) << "another deployer process is running; aborting operation.";
    CloseHandle(hMutex);
    // MessageBox(NULL, L"正在執行另一項部署任務，請稍候再試。", L"【小鹤】",
    // MB_OK | MB_ICONINFORMATION);
    MSG_BY_IDS(IDS_STR_DEPLOYING_WAIT, IDS_STR_WEASEL,
               MB_OK | MB_ICONINFORMATION);
    return 1;
  }

  weasel::Client client;
  if (client.Connect()) {
    LOG(INFO) << "Turning WeaselServer into maintenance mode.";
    client.StartMaintenance();
  }

  {
    RimeApi* rime = rime_get_api();
    if (!rime->sync_user_data()) {
      LOG(ERROR) << "Error synching user data.";
      CloseHandle(hMutex);
      return 1;
    }
    rime->join_maintenance_thread();
  }

  CloseHandle(hMutex);  // should be closed before resuming service.

  if (client.Connect()) {
    LOG(INFO) << "Resuming service.";
    client.EndMaintenance();
  }
  return 0;
}
