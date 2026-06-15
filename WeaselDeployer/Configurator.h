#pragma once

class UIStyleSettings;

class Configurator {
 public:
  explicit Configurator();

  void Initialize();
  int Run(bool installing);
  int UpdateWorkspace(bool report_errors = false);
  int DictManagement();
  int CustomPhraseEntry();
  int CommonPhraseEdit();
  int HotkeySettings();
  int SyncUserData();
};
