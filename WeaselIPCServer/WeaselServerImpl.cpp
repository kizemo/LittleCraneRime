#include "stdafx.h"
#include "WeaselServerImpl.h"
#include <mutex>
#include <Windows.h>
#include <resource.h>
#include <WeaselUtility.h>
#include <WeaselFileLog.h>
#include <WeaselGlobalHotkeys.h>

namespace weasel {
class PipeServer : public PipeChannel<DWORD, PipeMessage> {
 public:
  using ServerRunner = std::function<void()>;
  using Respond = std::function<void(Msg)>;
  using ServerHandler = std::function<void(PipeMessage, Respond)>;

  PipeServer(std::wstring&& pn_cmd, SECURITY_ATTRIBUTES* s);

 public:
  void Listen(ServerHandler const& handler);
  /* Get a server runner */
  ServerRunner GetServerRunner(ServerHandler const& handler);

 private:
  void _ProcessPipeThread(HANDLE pipe, ServerHandler const& handler);
};
}  // namespace weasel

using namespace weasel;

extern CAppModule _Module;

static LONG g_quickbar_x = -1;
static LONG g_quickbar_y = -1;
static HANDLE g_weasel_instance_mutex = NULL;

ServerImpl::ServerImpl()
    : m_pRequestHandler(NULL),
      m_darkMode(IsUserDarkMode()),
      channel(std::make_unique<PipeServer>(GetPipeName(), sa.get_attr())) {
  m_hUser32Module = GetModuleHandle(_T("user32.dll"));
}

ServerImpl::~ServerImpl() {
  _Finailize();
}

void ServerImpl::_Finailize() {
  if (pipeThread != nullptr) {
    pipeThread->interrupt();
    pipeThread = nullptr;
  } else {
    // avoid finalize again
    return;
  }

  if (IsWindow()) {
    DestroyWindow();
  }
}

LRESULT ServerImpl::OnColorChange(UINT uMsg,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  BOOL& bHandled) {
  if (IsUserDarkMode() != m_darkMode) {
    m_darkMode = IsUserDarkMode();
    m_pRequestHandler->UpdateColorTheme(m_darkMode);
  }
  return 0;
}

LRESULT ServerImpl::OnCreate(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled) {
  ::SetWindowText(m_hWnd, WEASEL_IPC_WINDOW);
  weasel::global_hotkey::RegisterPanelHotkeys(m_hWnd);
  return 0;
}

LRESULT ServerImpl::OnClose(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled) {
  Stop();
  return 0;
}

LRESULT ServerImpl::OnDestroy(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled) {
  weasel::global_hotkey::UnregisterPanelHotkeys(m_hWnd);
  bHandled = FALSE;
  return 1;
}

LRESULT ServerImpl::OnTrayNotify(UINT uMsg,
                               WPARAM wParam,
                               LPARAM lParam,
                               BOOL& bHandled) {
  if (m_onTrayNotify) {
    bHandled = TRUE;
    return m_onTrayNotify(wParam, lParam);
  }
  bHandled = FALSE;
  return 0;
}

LRESULT ServerImpl::OnHotKey(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled) {
  weasel::global_hotkey::RegisterPanelHotkeys(m_hWnd);
  if (wParam == weasel::global_hotkey::kIdQuickBar) {
    ::PostMessage(m_hWnd, WM_WEASEL_SHOW_QUICKBAR, 0, 0);
    return 0;
  }
  if (wParam == weasel::global_hotkey::kIdCommonPhrase) {
    ::PostMessage(m_hWnd, WM_WEASEL_SHOW_COMMON_PHRASES, 0, 0);
    return 0;
  }
  bHandled = FALSE;
  return 0;
}

LRESULT ServerImpl::OnQueryEndSystemSession(UINT uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            BOOL& bHandled) {
  return TRUE;
}

LRESULT ServerImpl::OnEndSystemSession(UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       BOOL& bHandled) {
  if (m_pRequestHandler) {
    m_pRequestHandler->Finalize();
    m_pRequestHandler = nullptr;
  }
  return 0;
}

namespace {

void LogThrottledAsciiBlock(const wchar_t* tag,
                            const wchar_t* fmt,
                            unsigned long id,
                            unsigned long session) {
  static DWORD s_last_log = 0;
  static unsigned s_suppressed = 0;
  const DWORD now = GetTickCount();
  if (now - s_last_log >= 1000) {
    wchar_t abuf[128];
    swprintf_s(abuf, fmt, id, session, s_suppressed);
    WeaselAppendLogW(L"weasel.server.log", tag, abuf);
    s_last_log = now;
    s_suppressed = 0;
  } else {
    s_suppressed++;
  }
}

}  // namespace

LRESULT ServerImpl::OnCommand(UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL& bHandled) {
  UINT uID = LOWORD(wParam);

  switch (uID) {
    case ID_WEASELTRAY_ENABLE_ASCII:
    case ID_WEASELTRAY_DISABLE_ASCII: {
      const bool user_initiated = (lParam & WEASEL_TRAY_USER_INITIATED) != 0;
      const DWORD sid = lParam & ~WEASEL_TRAY_USER_INITIATED;
      if (!user_initiated) {
        LogThrottledAsciiBlock(
            L"ascii-guard", L"block tray ascii cmd id=%lu session=%lu suppressed=%u",
            static_cast<unsigned long>(uID), static_cast<unsigned long>(sid));
        return 0;
      }
      if (m_pRequestHandler) {
        m_pRequestHandler->SetOption(sid, "ascii_mode",
                                     uID == ID_WEASELTRAY_ENABLE_ASCII);
      }
      return 0;
    }
    default:;
  }

  wchar_t logbuf[128];
  swprintf_s(logbuf, L"UI thread tray cmd id=%u session=%lu", uID,
             static_cast<unsigned long>(lParam));
  WeaselAppendLogW(L"weasel.server.log", L"tray", logbuf);

  std::map<UINT, CommandHandler>::iterator it = m_MenuHandlers.find(uID);
  if (it == m_MenuHandlers.end()) {
    bHandled = FALSE;
    return 0;
  }
  it->second();
  return 0;
}

DWORD ServerImpl::OnCommand(WEASEL_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  if (!m_hWnd)
    return FALSE;
  if (wParam == ID_WEASELTRAY_QUICK_BAR) {
    LONG x = static_cast<LONG>(static_cast<SHORT>(LOWORD(lParam)));
    LONG y = static_cast<LONG>(static_cast<SHORT>(HIWORD(lParam)));
    if (x > 0 || y > 0) {
      g_quickbar_x = x;
      g_quickbar_y = y;
    }
    WeaselAppendLogW(L"weasel.server.log", L"ipc", L"post show quickbar");
    ::PostMessage(m_hWnd, WM_WEASEL_SHOW_QUICKBAR, 0, 0);
    return TRUE;
  }
  if (wParam == ID_WEASELTRAY_COMMON_PHRASES) {
    ::PostMessage(m_hWnd, WM_WEASEL_SHOW_COMMON_PHRASES, 0, 0);
    return TRUE;
  }
  if (wParam == ID_WEASELTRAY_ENABLE_ASCII ||
      wParam == ID_WEASELTRAY_DISABLE_ASCII) {
    if (!(lParam & WEASEL_TRAY_USER_INITIATED)) {
      LogThrottledAsciiBlock(
          L"ascii-guard", L"ipc block tray ascii id=%lu session=%lu suppressed=%u",
          static_cast<unsigned long>(wParam),
          static_cast<unsigned long>(lParam & ~WEASEL_TRAY_USER_INITIATED));
      return TRUE;
    }
    if (m_pRequestHandler) {
      const DWORD sid = lParam & ~WEASEL_TRAY_USER_INITIATED;
      const bool enable = (wParam == ID_WEASELTRAY_ENABLE_ASCII);
      auto eat = [this](std::wstring& msg) -> bool {
        *channel << msg;
        return true;
      };
      m_pRequestHandler->SetOption(sid, "ascii_mode", enable);
      m_pRequestHandler->Respond(sid, eat);
    }
    return TRUE;
  }
  if (wParam == ID_WEASELTRAY_TOGGLE_ASCII) {
    if (!(lParam & WEASEL_TRAY_USER_INITIATED))
      return TRUE;
    if (m_pRequestHandler) {
      const DWORD sid = lParam & ~WEASEL_TRAY_USER_INITIATED;
      auto eat = [this](std::wstring& msg) -> bool {
        *channel << msg;
        return true;
      };
      m_pRequestHandler->ToggleOption(sid, "ascii_mode", eat);
    }
    return TRUE;
  }
  wchar_t logbuf[128];
  swprintf_s(logbuf, L"post tray cmd id=%lu session=%lu",
             static_cast<unsigned long>(wParam),
             static_cast<unsigned long>(lParam));
  WeaselAppendLogW(L"weasel.server.log", L"ipc", logbuf);
  ::PostMessage(m_hWnd, WM_COMMAND, wParam, lParam);
  return TRUE;
}

LRESULT ServerImpl::OnShowQuickBar(UINT uMsg,
                                   WPARAM wParam,
                                   LPARAM lParam,
                                   BOOL& bHandled) {
  POINT pt = TakeQuickBarAnchor();
  if (pt.x < 0 || pt.y < 0)
    GetCursorPos(&pt);
  WeaselAppendLogW(L"weasel.server.log", L"quickbar", L"OnShowQuickBar UI");
  if (m_onShowQuickBar)
    m_onShowQuickBar(pt);
  else {
    auto it = m_MenuHandlers.find(ID_WEASELTRAY_QUICK_BAR);
    if (it != m_MenuHandlers.end())
      it->second();
  }
  return 0;
}

LRESULT ServerImpl::OnShowCommonPhrases(UINT uMsg,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        BOOL& bHandled) {
  if (m_onShowCommonPhrases)
    m_onShowCommonPhrases();
  else {
    auto it = m_MenuHandlers.find(ID_WEASELTRAY_COMMON_PHRASES);
    if (it != m_MenuHandlers.end())
      it->second();
  }
  return 0;
}

int ServerImpl::Start() {
  std::wstring instanceName = L"(WEASEL)Furandōru-Sukāretto-";
  instanceName += getUsername();
  g_weasel_instance_mutex = ::CreateMutexW(NULL, TRUE, instanceName.c_str());
  const DWORD mutex_err = ::GetLastError();
  if (mutex_err == ERROR_ALREADY_EXISTS || mutex_err == ERROR_ACCESS_DENIED) {
    if (g_weasel_instance_mutex) {
      ::CloseHandle(g_weasel_instance_mutex);
      g_weasel_instance_mutex = NULL;
    }
    return 0;  // assure single instance
  }

  HWND hwnd = Create(NULL);

  return (int)hwnd;
}

int ServerImpl::Stop() {
  // DO NOT exit process or finalize here
  // Let WeaselServer handle this
  PostMessage(WM_QUIT);
  return 0;
}

static std::mutex g_api_mutex;

POINT ServerImpl::TakeQuickBarAnchor() {
  POINT pt = {g_quickbar_x, g_quickbar_y};
  g_quickbar_x = -1;
  g_quickbar_y = -1;
  return pt;
}

int ServerImpl::Run() {
  // This workaround causes a VC internal error:
  // void PipeServer::Listen(ServerHandler handler);
  //
  // auto handler = boost::bind(&ServerImpl::HandlePipeMessage, this);
  // auto listener = boost::bind(&PipeServer::Listen, channel.get(), handler);
  //
  auto listener = [this](PipeMessage msg, PipeServer::Respond resp) -> void {
    std::lock_guard guard(g_api_mutex);
    HandlePipeMessage(msg, resp);
  };
  pipeThread = std::make_unique<boost::thread>(
      [this, &listener]() { channel->Listen(listener); });

  CMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);
  int nRet = theLoop.Run();
  _Module.RemoveMessageLoop();
  return nRet;
}

DWORD ServerImpl::OnEcho(WEASEL_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->FindSession(lParam);
}

DWORD ServerImpl::OnStartSession(WEASEL_IPC_COMMAND uMsg,
                                 DWORD wParam,
                                 DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->AddSession(
      reinterpret_cast<LPWSTR>(channel->ReceiveBuffer()),
      [this](std::wstring& msg) -> bool {
        *channel << msg;
        return true;
      });
}

DWORD ServerImpl::OnEndSession(WEASEL_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->RemoveSession(lParam);
}

DWORD ServerImpl::OnKeyEvent(WEASEL_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler /* || !m_pSharedMemory*/)
    return 0;

  auto eat = [this](std::wstring& msg) -> bool {
    *channel << msg;
    return true;
  };
  return m_pRequestHandler->ProcessKeyEvent(KeyEvent(wParam), lParam, eat);
}

DWORD ServerImpl::OnShutdownServer(WEASEL_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  Stop();
  return 0;
}

DWORD ServerImpl::OnFocusIn(WEASEL_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusIn(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnFocusOut(WEASEL_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusOut(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnUpdateInputPosition(WEASEL_IPC_COMMAND uMsg,
                                        DWORD wParam,
                                        DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  /*
   * 移位标志 = 1bit == 0
   * height: 0~127 = 7bit
   * top:-2048~2047 = 12bit（有符号）
   * left:-2048~2047 = 12bit（有符号）
   *
   * 高解析度下：
   * 移位标志 = 1bit == 1
   * height: 0~254 = 7bit（舍弃低1位）
   * top: -4096~4094 = 12bit（有符号，舍弃低1位）
   * left: -4096~4094 = 12bit（有符号，舍弃低1位）
   */
  RECT rc;
  int hi_res = (wParam >> 31) & 0x01;
  rc.left = ((wParam & 0x7ff) - (wParam & 0x800)) << hi_res;
  rc.top = (((wParam >> 12) & 0x7ff) - ((wParam >> 12) & 0x800)) << hi_res;
  const int width = 6;
  int height = ((wParam >> 24) & 0x7f) << hi_res;
  rc.right = rc.left + width;
  rc.bottom = rc.top + height;

  {
    using PPTLPFPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
    PPTLPFPMDPI PhysicalToLogicalPointForPerMonitorDPI =
        (PPTLPFPMDPI)::GetProcAddress(m_hUser32Module,
                                      "PhysicalToLogicalPointForPerMonitorDPI");
    POINT lt = {rc.left, rc.top};
    POINT rb = {rc.right, rc.bottom};
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &lt);
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &rb);
    rc = {lt.x, lt.y, rb.x, rb.y};
  }

  m_pRequestHandler->UpdateInputPosition(rc, lParam);
  return 0;
}

DWORD ServerImpl::OnStartMaintenance(WEASEL_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->StartMaintenance();
  return 0;
}

DWORD ServerImpl::OnEndMaintenance(WEASEL_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->EndMaintenance();
  return 0;
}

DWORD ServerImpl::OnCommitComposition(WEASEL_IPC_COMMAND uMsg,
                                      DWORD wParam,
                                      DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->CommitComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnClearComposition(WEASEL_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->ClearComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnSelectCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                 DWORD wParam,
                                                 DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->SelectCandidateOnCurrentPage(wParam, lParam, eat);
  }
  return 0;
}

DWORD ServerImpl::OnHighlightCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                    DWORD wParam,
                                                    DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->HighlightCandidateOnCurrentPage(wParam, lParam, eat);
  }
  return 0;
}

DWORD ServerImpl::OnChangePage(WEASEL_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->ChangePage(wParam, lParam, eat);
  }
  return 0;
}

DWORD ServerImpl::OnCommitText(WEASEL_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  auto eat = [this](std::wstring& msg) -> bool {
    *channel << msg;
    return true;
  };
  return m_pRequestHandler->CommitText(
      reinterpret_cast<LPWSTR>(channel->ReceiveBuffer()), lParam, eat);
}

DWORD ServerImpl::OnDeleteCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                 DWORD wParam,
                                                 DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    const bool ok =
        m_pRequestHandler->DeleteCandidateOnCurrentPage(wParam, lParam, eat);
    wchar_t buf[96];
    swprintf_s(buf, L"DeleteCandidate index=%lu session=%lu ok=%d", wParam,
               lParam, ok ? 1 : 0);
    WeaselAppendLogW(L"weasel.server.log", L"delete", buf);
    return ok ? 1 : 0;
  }
  return 0;
}

#define MAP_PIPE_MSG_HANDLE(__msg, __wParam, __lParam) \
  {                                                    \
    auto lParam = __lParam;                            \
    auto wParam = __wParam;                            \
    LRESULT _result = 0;                               \
    switch (__msg) {
#define PIPE_MSG_HANDLE(__msg, __func)       \
  case __msg:                                \
    _result = __func(__msg, wParam, lParam); \
    break;

#define END_MAP_PIPE_MSG_HANDLE(__result) \
  }                                       \
  __result = _result;                     \
  }

template <typename _Resp>
void ServerImpl::HandlePipeMessage(PipeMessage pipe_msg, _Resp resp) {
  DWORD result;

  MAP_PIPE_MSG_HANDLE(pipe_msg.Msg, pipe_msg.wParam, pipe_msg.lParam)
  PIPE_MSG_HANDLE(WEASEL_IPC_ECHO, OnEcho)
  PIPE_MSG_HANDLE(WEASEL_IPC_START_SESSION, OnStartSession)
  PIPE_MSG_HANDLE(WEASEL_IPC_END_SESSION, OnEndSession)
  PIPE_MSG_HANDLE(WEASEL_IPC_PROCESS_KEY_EVENT, OnKeyEvent)
  PIPE_MSG_HANDLE(WEASEL_IPC_SHUTDOWN_SERVER, OnShutdownServer)
  PIPE_MSG_HANDLE(WEASEL_IPC_FOCUS_IN, OnFocusIn)
  PIPE_MSG_HANDLE(WEASEL_IPC_FOCUS_OUT, OnFocusOut)
  PIPE_MSG_HANDLE(WEASEL_IPC_UPDATE_INPUT_POS, OnUpdateInputPosition)
  PIPE_MSG_HANDLE(WEASEL_IPC_START_MAINTENANCE, OnStartMaintenance)
  PIPE_MSG_HANDLE(WEASEL_IPC_END_MAINTENANCE, OnEndMaintenance)
  PIPE_MSG_HANDLE(WEASEL_IPC_COMMIT_COMPOSITION, OnCommitComposition)
  PIPE_MSG_HANDLE(WEASEL_IPC_CLEAR_COMPOSITION, OnClearComposition);
  PIPE_MSG_HANDLE(WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
                  OnSelectCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
                  OnHighlightCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(WEASEL_IPC_CHANGE_PAGE, OnChangePage);
  PIPE_MSG_HANDLE(WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,
                  OnDeleteCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(WEASEL_IPC_COMMIT_TEXT, OnCommitText);
  PIPE_MSG_HANDLE(WEASEL_IPC_TRAY_COMMAND, OnCommand);
  END_MAP_PIPE_MSG_HANDLE(result);

  resp(result);
}

PipeServer::PipeServer(std::wstring&& pn_cmd, SECURITY_ATTRIBUTES* s)
    : PipeChannel(std::move(pn_cmd), s) {}

void PipeServer::Listen(ServerHandler const& handler) {
  for (;;) {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    try {
      boost::this_thread::interruption_point();
      pipe = _ConnectServerPipe(pname);
      boost::thread th(
          [&handler, pipe, this] { _ProcessPipeThread(pipe, handler); });
    } catch (DWORD ex) {
      _FinalizePipe(pipe);
    }
    boost::this_thread::interruption_point();
  }
}

PipeServer::ServerRunner PipeServer::GetServerRunner(
    ServerHandler const& handler) {
  return [&handler, this]() { Listen(handler); };
}

void PipeServer::_ProcessPipeThread(HANDLE pipe, ServerHandler const& handler) {
  try {
    for (;;) {
      Res msg;
      _Receive(pipe, &msg, sizeof(msg));
      handler(msg, [this, pipe](Msg resp) { _Send(pipe, resp); });
    }
  } catch (...) {
    _FinalizePipe(pipe);
  }
}

// weasel::Server

Server::Server() : m_pImpl(new ServerImpl) {}

Server::~Server() {
  if (m_pImpl)
    delete m_pImpl;
}

int Server::Start() {
  return m_pImpl->Start();
}

int Server::Stop() {
  return m_pImpl->Stop();
}

int Server::Run() {
  return m_pImpl->Run();
}

void Server::SetRequestHandler(RequestHandler* pHandler) {
  m_pImpl->SetRequestHandler(pHandler);
}

void Server::AddMenuHandler(UINT uID, CommandHandler handler) {
  m_pImpl->AddMenuHandler(uID, handler);
}

HWND Server::GetHWnd() {
  return m_pImpl->m_hWnd;
}

POINT Server::TakeQuickBarAnchor() {
  return ServerImpl::TakeQuickBarAnchor();
}

void Server::SetQuickBarHandler(std::function<void(POINT)> handler) {
  m_pImpl->SetQuickBarHandler(std::move(handler));
}

void Server::SetCommonPhraseHandler(std::function<void()> handler) {
  m_pImpl->SetCommonPhraseHandler(std::move(handler));
}

void Server::SetTrayNotifyHandler(std::function<LRESULT(WPARAM, LPARAM)> handler) {
  m_pImpl->SetTrayNotifyHandler(std::move(handler));
}
