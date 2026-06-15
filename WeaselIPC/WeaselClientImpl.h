#pragma once
#include <WeaselIPC.h>
#include <PipeChannel.h>

namespace weasel {

class ClientImpl {
 public:
  ClientImpl();
  ~ClientImpl();

  bool Connect(ServerLauncher const& launcher);
  void Disconnect();
  void ShutdownServer();
  void StartSession();
  void EndSession();
  void StartMaintenance();
  void EndMaintenance();
  bool EnsureSession();
  DWORD SessionId() const { return session_id; }
  bool Echo();
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  bool CommitComposition();
  bool ClearComposition();
  bool SelectCandidateOnCurrentPage(size_t index);
  bool HighlightCandidateOnCurrentPage(size_t index);
  bool ChangePage(bool backward);
  bool DeleteCandidateOnCurrentPage(size_t index);
  void UpdateInputPosition(RECT const& rc);
  void FocusIn();
  void FocusOut();
  void TrayCommand(UINT menuId, DWORD param = 0);
  void SetAsciiMode(bool ascii);
  bool ToggleAsciiMode();
  bool CommitText(const std::wstring& text);
  void SetNotifyHwnd(HWND hwnd);
  bool GetResponseData(ResponseHandler const& handler);

 protected:
  void _InitializeClientInfo();
  bool _WriteClientInfo();

  LRESULT _SendMessage(WEASEL_IPC_COMMAND Msg, DWORD wParam, DWORD lParam);

  bool _Connected() const { return channel.Connected(); }
  bool _Active() const { return channel.Connected() && session_id != 0; }

 private:
  UINT session_id;
  std::wstring app_name;
  bool is_ime;
  HWND notify_hwnd_ = NULL;
  bool notify_dirty_ = false;

  PipeChannel<PipeMessage> channel;
};

}  // namespace weasel