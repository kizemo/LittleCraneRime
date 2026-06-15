#include "stdafx.h"
#include "WeaselClientImpl.h"
#include <resource.h>
#include <StringAlgorithm.hpp>

using namespace weasel;

ClientImpl::ClientImpl()
    : session_id(0), channel(GetPipeName()), is_ime(false) {
  _InitializeClientInfo();
}

ClientImpl::~ClientImpl() {
  if (channel.Connected())
    Disconnect();
}

// http://stackoverflow.com/questions/557081/how-do-i-get-the-hmodule-for-the-currently-executing-code
HMODULE GetCurrentModule() {  // NB: XP+ solution!
  HMODULE hModule = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    (LPCTSTR)GetCurrentModule, &hModule);

  return hModule;
}

void ClientImpl::_InitializeClientInfo() {
  // get app name
  WCHAR exe_path[MAX_PATH] = {0};
  GetModuleFileName(NULL, exe_path, MAX_PATH);
  std::wstring path = exe_path;
  size_t separator_pos = path.find_last_of(L"\\/");
  if (separator_pos < path.size())
    app_name = path.substr(separator_pos + 1);
  else
    app_name = path;
  to_lower(app_name);
  // determine client type
  GetModuleFileName(GetCurrentModule(), exe_path, MAX_PATH);
  path = exe_path;
  to_lower(path);
  is_ime = ends_with(path, L".ime");
}

bool ClientImpl::Connect(ServerLauncher const& launcher) {
  return channel.Connect();
}

void ClientImpl::Disconnect() {
  try {
    if (_Active())
      EndSession();
  } catch (...) {
    session_id = 0;
  }
  try {
    channel.Disconnect();
  } catch (...) {
  }
}

void ClientImpl::ShutdownServer() {
  _SendMessage(WEASEL_IPC_SHUTDOWN_SERVER, 0, 0);
}

bool ClientImpl::ProcessKeyEvent(KeyEvent const& keyEvent) {
  if (!_Active())
    return false;

  LRESULT ret =
      _SendMessage(WEASEL_IPC_PROCESS_KEY_EVENT, keyEvent, session_id);
  return ret != 0;
}

bool ClientImpl::CommitComposition() {
  if (!_Active())
    return false;

  LRESULT ret = _SendMessage(WEASEL_IPC_COMMIT_COMPOSITION, 0, session_id);
  return ret != 0;
}

bool ClientImpl::ClearComposition() {
  if (!_Active())
    return false;

  LRESULT ret = _SendMessage(WEASEL_IPC_CLEAR_COMPOSITION, 0, session_id);
  return ret != 0;
}

bool ClientImpl::SelectCandidateOnCurrentPage(size_t index) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE, index,
                             session_id);
  return ret != 0;
}

bool ClientImpl::HighlightCandidateOnCurrentPage(size_t index) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
                             index, session_id);
  return ret != 0;
}

bool ClientImpl::ChangePage(bool backward) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_CHANGE_PAGE, backward, session_id);
  return ret != 0;
}

bool ClientImpl::DeleteCandidateOnCurrentPage(size_t index) {
  if (!_Active())
    return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,
                             static_cast<DWORD>(index), session_id);
  return ret != 0;
}

bool ClientImpl::CommitText(const std::wstring& text) {
  if (!_Active() || text.empty())
    return false;
  channel << L"action=commit_text\n";
  channel << L"text=" << text << L"\n";
  channel << L".\n";
  LRESULT ret = _SendMessage(WEASEL_IPC_COMMIT_TEXT, 0, session_id);
  return ret != 0;
}

void ClientImpl::SetNotifyHwnd(HWND hwnd) {
  if (notify_hwnd_ != hwnd) {
    notify_hwnd_ = hwnd;
    notify_dirty_ = true;
  }
}

void ClientImpl::UpdateInputPosition(RECT const& rc) {
  if (!_Active())
    return;
  /*
  移位标志 = 1bit == 0
  height:0~127 = 7bit
  top:-2048~2047 = 12bit（有符号）
  left:-2048~2047 = 12bit（有符号）

  高解析度下：
  移位标志 = 1bit == 1
  height:0~254 = 7bit（舍弃低1位）
  top:-4096~4094 = 12bit（有符号，舍弃低1位）
  left:-4096~4094 = 12bit（有符号，舍弃低1位）
  */
  int hi_res = 0;
  if (rc.bottom - rc.top >= 128 || abs(rc.left) > 2047 || abs(rc.top) > 2047)
    hi_res = 1;
  int left = rc.left >> hi_res;
  int top = rc.top >> hi_res;
  left = max(-2048, min(2047, left));
  top = max(-2048, min(2047, top));
  int height = max(0, min(127, (rc.bottom - rc.top) >> hi_res));
  DWORD compressed_rect = ((hi_res & 0x01) << 31) | ((height & 0x7f) << 24) |
                          ((top & 0xfff) << 12) | (left & 0xfff);
  _SendMessage(WEASEL_IPC_UPDATE_INPUT_POS, compressed_rect, session_id);
}

void ClientImpl::FocusIn() {
  DWORD client_caps = 0; /* TODO */
  _SendMessage(WEASEL_IPC_FOCUS_IN, client_caps, session_id);
}

void ClientImpl::FocusOut() {
  _SendMessage(WEASEL_IPC_FOCUS_OUT, 0, session_id);
}

void ClientImpl::TrayCommand(UINT menuId, DWORD param) {
  DWORD lParam = session_id;
  if (menuId == ID_WEASELTRAY_QUICK_BAR && param != 0)
    lParam = param;
  _SendMessage(WEASEL_IPC_TRAY_COMMAND, menuId, lParam);
}

void ClientImpl::SetAsciiMode(bool ascii) {
  if (!EnsureSession())
    return;
  const UINT cmd =
      ascii ? ID_WEASELTRAY_ENABLE_ASCII : ID_WEASELTRAY_DISABLE_ASCII;
  _SendMessage(WEASEL_IPC_TRAY_COMMAND, cmd,
               session_id | WEASEL_TRAY_USER_INITIATED);
}

bool ClientImpl::ToggleAsciiMode() {
  if (!EnsureSession())
    return false;
  _SendMessage(WEASEL_IPC_TRAY_COMMAND, ID_WEASELTRAY_TOGGLE_ASCII,
               session_id | WEASEL_TRAY_USER_INITIATED);
  return true;
}

bool ClientImpl::EnsureSession() {
  if (!channel.Connected())
    return false;
  if (session_id != 0 && Echo())
    return true;
  StartSession();
  return session_id != 0 && Echo();
}

void ClientImpl::StartSession() {
  if (_Active() && Echo() && !notify_dirty_)
    return;
  if (_Active() && notify_dirty_)
    EndSession();
  notify_dirty_ = false;

  _WriteClientInfo();
  UINT ret = _SendMessage(WEASEL_IPC_START_SESSION, 0, 0);
  session_id = ret;
}

void ClientImpl::EndSession() {
  try {
    if (_Active())
      _SendMessage(WEASEL_IPC_END_SESSION, 0, session_id);
  } catch (...) {
  }
  session_id = 0;
}

void ClientImpl::StartMaintenance() {
  _SendMessage(WEASEL_IPC_START_MAINTENANCE, 0, 0);
  session_id = 0;
}

void ClientImpl::EndMaintenance() {
  _SendMessage(WEASEL_IPC_END_MAINTENANCE, 0, 0);
  session_id = 0;
}

bool ClientImpl::Echo() {
  if (!_Active())
    return false;

  UINT serverEcho = _SendMessage(WEASEL_IPC_ECHO, 0, session_id);
  return (serverEcho == session_id);
}

bool ClientImpl::GetResponseData(ResponseHandler const& handler) {
  if (!handler) {
    return false;
  }

  try {
    return channel.HandleResponseData(handler);
  } catch (...) {
    return false;
  }
}

bool ClientImpl::_WriteClientInfo() {
  channel << L"action=session\n";
  channel << L"session.client_app=" << app_name.c_str() << L"\n";
  channel << L"session.client_type=" << (is_ime ? L"ime" : L"tsf") << L"\n";
  if (notify_hwnd_)
    channel << L"session.client_notify_hwnd=" << notify_hwnd_ << L"\n";
  channel << L".\n";
  return true;
}

LRESULT ClientImpl::_SendMessage(WEASEL_IPC_COMMAND Msg,
                                 DWORD wParam,
                                 DWORD lParam) {
  try {
    PipeMessage req{Msg, wParam, lParam};
    return channel.Transact(req);
  } catch (DWORD /* ex */) {
    return 0;
  }
}

Client::Client() : m_pImpl(new ClientImpl()) {}

Client::~Client() {
  if (m_pImpl)
    delete m_pImpl;
}

bool Client::Connect(ServerLauncher launcher) {
  return m_pImpl->Connect(launcher);
}

void Client::Disconnect() {
  m_pImpl->Disconnect();
}

void Client::ShutdownServer() {
  m_pImpl->ShutdownServer();
}

bool Client::ProcessKeyEvent(KeyEvent const& keyEvent) {
  return m_pImpl->ProcessKeyEvent(keyEvent);
}

bool Client::CommitComposition() {
  return m_pImpl->CommitComposition();
}

bool Client::ClearComposition() {
  return m_pImpl->ClearComposition();
}

bool Client::SelectCandidateOnCurrentPage(size_t index) {
  return m_pImpl->SelectCandidateOnCurrentPage(index);
}

bool Client::HighlightCandidateOnCurrentPage(size_t index) {
  return m_pImpl->HighlightCandidateOnCurrentPage(index);
}

bool Client::ChangePage(bool backward) {
  return m_pImpl->ChangePage(backward);
}

bool Client::DeleteCandidateOnCurrentPage(size_t index) {
  return m_pImpl->DeleteCandidateOnCurrentPage(index);
}

void Client::UpdateInputPosition(RECT const& rc) {
  m_pImpl->UpdateInputPosition(rc);
}

void Client::FocusIn() {
  m_pImpl->FocusIn();
}

void Client::FocusOut() {
  m_pImpl->FocusOut();
}

void Client::StartSession() {
  m_pImpl->StartSession();
}

void Client::EndSession() {
  m_pImpl->EndSession();
}

void Client::StartMaintenance() {
  m_pImpl->StartMaintenance();
}

void Client::EndMaintenance() {
  m_pImpl->EndMaintenance();
}

void Client::TrayCommand(UINT menuId, DWORD param) {
  m_pImpl->TrayCommand(menuId, param);
}

void Client::SetAsciiMode(bool ascii) {
  m_pImpl->SetAsciiMode(ascii);
}

bool Client::ToggleAsciiMode() {
  return m_pImpl->ToggleAsciiMode();
}

bool Client::Echo() {
  return m_pImpl->Echo();
}

bool Client::EnsureSession() {
  return m_pImpl->EnsureSession();
}

DWORD Client::SessionId() const {
  return m_pImpl->SessionId();
}

bool Client::GetResponseData(ResponseHandler handler) {
  return m_pImpl->GetResponseData(handler);
}

bool Client::CommitText(const std::wstring& text) {
  return m_pImpl->CommitText(text);
}

void Client::SetNotifyHwnd(HWND hwnd) {
  m_pImpl->SetNotifyHwnd(hwnd);
}
