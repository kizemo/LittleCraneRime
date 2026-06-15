#include "stdafx.h"

#include <WeaselIPCData.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include "Compartment.h"
#include "ResponseParser.h"
#include <WeaselMessages.h>
#include <WeaselConstants.h>
#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>
#include <WeaselTSFCallbackLog.h>
#include <WeaselTSFCallbackSafe.h>

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_weasel_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

WeaselTSF::WeaselTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _KeySinkReset();

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

WeaselTSF::~WeaselTSF() {
  DllRelease();
}

STDAPI WeaselTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) WeaselTSF::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) WeaselTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDAPI WeaselTSF::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

void WeaselTSF::_RequestIpcRefresh(BOOL urgent, BOOL force) {
  _ipc_refresh_requested = TRUE;
  if (urgent)
    _ipc_refresh_urgent = TRUE;
  if (force)
    _ipc_refresh_force = TRUE;
}

STDAPI WeaselTSF::Deactivate() {
  return weasel_tsf_safe::RunTSFCallback(L"Deactivate", [&]() -> HRESULT {
  weasel_crash_diag::Crumb(L"lifecycle", L"Deactivate begin");
  weasel_tsf_cb_log::Enter(L"Deactivate");
  try {
    try {
      m_client.Disconnect();
    } catch (...) {
      weasel_crash_diag::Crumb(L"lifecycle", L"Disconnect exception");
    }
    _server_connected = false;

    _edit_session_pending = FALSE;
  _composition_dirty = FALSE;
  _ipc_refresh_requested = FALSE;
  _ipc_refresh_urgent = FALSE;
  _ipc_refresh_force = FALSE;
  _end_composition_pending = FALSE;
  _edit_session_depth = 0;
  _deferred.end_requested = FALSE;
  _deferred.end_clear = TRUE;
  _deferred.end_context = nullptr;
  _deferred.layout_update = FALSE;

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitNotifyWindow();

  _UninitThreadMgrEventSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  try {
    _cand->DestroyAll();
  } catch (...) {
    weasel_crash_diag::Crumb(L"lifecycle", L"DestroyAll exception");
  }

  weasel_crash_diag::Crumb(L"lifecycle", L"Deactivate end");
    weasel_tsf_cb_log::Leave(L"Deactivate", L"OK");
  } catch (...) {
    weasel_crash_diag::Crumb(L"lifecycle", L"Deactivate uncaught");
    weasel_tsf_cb_log::Exception(L"Deactivate", L"uncaught (...) exception");
    weasel_tsf_cb_log::Leave(L"Deactivate", L"FAIL");
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                             TfClientId tfClientId,
                             DWORD dwFlags) {
  return weasel_tsf_safe::RunTSFCallback(L"ActivateEx", [&]() -> HRESULT {
  try {
    com_ptr<ITfDocumentMgr> pDocMgrFocus;
    _activateFlags = dwFlags;

    _pThreadMgr = pThreadMgr;
    _tfClientId = tfClientId;

    _edit_session_pending = FALSE;
    _composition_dirty = FALSE;
    _ipc_refresh_requested = FALSE;
    _ipc_refresh_urgent = FALSE;
    _ipc_refresh_force = FALSE;
    _end_composition_pending = FALSE;
    _edit_session_depth = 0;
    _deferred.end_requested = FALSE;
    _deferred.end_clear = TRUE;
    _deferred.end_context = nullptr;
    _deferred.layout_update = FALSE;

    if (!_InitThreadMgrEventSink())
      goto ExitError;

    if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
        (pDocMgrFocus != NULL)) {
      _InitTextEditSink(pDocMgrFocus);
    }

    if (!_InitKeyEventSink())
      goto ExitError;

    // if (!_InitDisplayAttributeGuidAtom())
    //	goto ExitError;
    //	some app might init failed because it not provide DisplayAttributeInfo,
    // like some opengl stuff
    _InitDisplayAttributeGuidAtom();

    if (!_InitPreservedKey())
      goto ExitError;

    if (!_InitLanguageBar())
      goto ExitError;

    if (!_IsKeyboardOpen())
      _SetKeyboardOpen(TRUE);

    if (!_InitCompartment())
      goto ExitError;
    if (!_InitThreadFocusSink())
      goto ExitError;

    if (!_InitNotifyWindow())
      goto ExitError;

    m_client.SetNotifyHwnd(_notify_hwnd);
    _EnsureIpcSession();
    _EnsureStyleFromSession();
    WeaselAppendLogW(L"weasel.tsf.log", L"startup",
                     L"WeaselTSF " WEASEL_VERSION L" ActivateEx");
    weasel_crash_diag::Crumb(L"lifecycle", L"ActivateEx");
    weasel_crash_diag::SetSnapshot(_edit_session_depth, _status.composing ? TRUE : FALSE,
                                   _server_connected ? TRUE : FALSE,
                                   static_cast<DWORD>(m_client.SessionId()));
    weasel_tsf_cb_log::Leave(L"ActivateEx", L"OK");
    return S_OK;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"lifecycle",
                     L"uncaught exception in ActivateEx");
    weasel_crash_diag::Crumb(L"lifecycle", L"ActivateEx ex");
    weasel_tsf_cb_log::Exception(L"ActivateEx", L"uncaught (...) exception");
    weasel_tsf_cb_log::Leave(L"ActivateEx", L"FAIL");
  }
ExitError:
  Deactivate();
  return E_FAIL;
  });
}

STDMETHODIMP WeaselTSF::OnSetThreadFocus() {
  return weasel_tsf_safe::RunTSFCallback(L"OnSetThreadFocus", [&]() -> HRESULT {
  weasel_tsf_cb_log::Enter(L"OnSetThreadFocus");
  try {
    std::wstring _ToggleImeOnOpenClose{};
    RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                      L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
    _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
    if (m_client.Echo()) {
      _server_connected = true;
      _SyncLanguageBarTrayVisibility();
    }
    weasel_tsf_cb_log::Leave(L"OnSetThreadFocus", L"OK");
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"focus",
                     L"uncaught exception in OnSetThreadFocus");
    weasel_crash_diag::Crumb(L"focus", L"OnSetThreadFocus ex");
    weasel_tsf_cb_log::Exception(L"OnSetThreadFocus", L"uncaught (...) exception");
    weasel_tsf_cb_log::Leave(L"OnSetThreadFocus", L"FAIL");
  }
  return S_OK;
  });
}
STDMETHODIMP WeaselTSF::OnKillThreadFocus() {
  return weasel_tsf_safe::RunTSFCallback(L"OnKillThreadFocus", [&]() -> HRESULT {
  weasel_tsf_cb_log::Enter(L"OnKillThreadFocus");
  try {
    _HideUI();
    _key_pull.valid = FALSE;
    _edit_session_pending = FALSE;
    _KeySinkReset();
    _ipc_refresh_requested = FALSE;
    _ipc_refresh_urgent = FALSE;
    _ipc_refresh_force = FALSE;
    _composition_dirty = FALSE;
    _deferred.end_requested = FALSE;
    _deferred.end_context = nullptr;
    _deferred.layout_update = FALSE;

    if (_IsComposing()) {
      com_ptr<ITfContext> ctx = _pEditSessionContext;
      if (!ctx)
        ctx = _GetFocusedTfContext();
      if (ctx)
        _QueueEndCompositionEditSession(ctx, FALSE);
      else
        _FinalizeComposition();
    }

    try {
      m_client.ClearComposition();
    } catch (...) {
      weasel_crash_diag::Crumb(L"focus", L"ClearComposition exception");
    }
    _committed = TRUE;
    weasel_crash_diag::Crumb(L"focus", L"KillThreadFocus ok");
    weasel_tsf_cb_log::Leave(L"OnKillThreadFocus", L"OK");
  } catch (...) {
    weasel_crash_diag::Crumb(L"focus", L"KillThreadFocus exception");
    weasel_tsf_cb_log::Exception(L"OnKillThreadFocus", L"uncaught (...) exception");
    weasel_tsf_cb_log::Leave(L"OnKillThreadFocus", L"FAIL");
  }
  return S_OK;
  });
}
BOOL WeaselTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}
void WeaselTSF::_UninitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    return;
}

STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  return weasel_tsf_safe::RunTSFCallback(L"OnActivated", [&]() -> HRESULT {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  if (isActivated) {
    _UpdateLanguageBar(_status);
    _SyncLanguageBarTrayVisibility();
  } else {
    _DeleteCandidateList();
    _ShowLanguageBar(FALSE);
  }
  return S_OK;
  });
}

void WeaselTSF::_Reconnect() {
  weasel_crash_diag::Crumb(L"ipc", L"Reconnect begin");
  _edit_session_pending = FALSE;
  _composition_dirty = FALSE;
  _ipc_refresh_requested = FALSE;
  _ipc_refresh_urgent = FALSE;
  _ipc_refresh_force = FALSE;
  _edit_session_depth = 0;
  _end_composition_pending = FALSE;
  // 0.18.99.1: IPC 重连时清空 ascii-toggle 锁，避免跨进程残留状态
  _ascii_toggle_tick_ = 0;
  _ascii_toggle_lock_rime_ = FALSE;
  if (_IsComposing())
    _FinalizeComposition();
  try {
    m_client.Disconnect();
  } catch (...) {
    weasel_crash_diag::Crumb(L"ipc", L"Reconnect Disconnect exception");
  }
  _server_connected = false;
  if (!m_client.Connect(NULL)) {
    WeaselAppendLogW(L"weasel.tsf.log", L"ipc", L"Reconnect Connect failed");
    weasel_crash_diag::Crumb(L"ipc", L"Reconnect Connect failed");
    return;
  }
  m_client.SetNotifyHwnd(_notify_hwnd);
  if (!m_client.EnsureSession()) {
    wchar_t buf[96];
    swprintf_s(buf, L"Reconnect EnsureSession failed session=%lu",
               static_cast<unsigned long>(m_client.SessionId()));
    WeaselAppendLogW(L"weasel.tsf.log", L"ipc", buf);
    return;
  }
  _EnsureStyleFromSession();
  _server_connected = true;
}

void WeaselTSF::_EnsureStyleFromSession() {
  if (_cand->style().font_point > 0)
    return;
  try {
    if (m_client.SessionId() != 0)
      m_client.EndSession();
    m_client.StartSession();
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    if (m_client.GetResponseData(std::ref(parser))) {
      if (_cand->style().font_point > 0)
        _cand->UpdateStyle(_cand->style());
      _UpdateLanguageBar(_status);
      weasel_crash_diag::Crumb(L"ipc", L"style drain ok");
      wchar_t buf[48];
      swprintf_s(buf, L"style drain font=%d", _cand->style().font_point);
      WeaselAppendLogW(L"weasel.tsf.log", L"ipc", buf);
    } else {
      weasel_crash_diag::Crumb(L"ipc", L"style drain miss");
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"ipc", L"style drain exception");
    weasel_crash_diag::Crumb(L"ipc", L"style drain exception");
  }
}

bool WeaselTSF::_EnsureIpcSession() {
  if (!_EnsureServerConnected())
    return false;
  if (m_client.EnsureSession())
    return true;
  wchar_t buf[96];
  swprintf_s(buf, L"EnsureSession failed session=%lu, reconnecting",
             static_cast<unsigned long>(m_client.SessionId()));
  WeaselAppendLogW(L"weasel.tsf.log", L"ipc", buf);
  _Reconnect();
  if (!m_client.EnsureSession()) {
    swprintf_s(buf, L"EnsureSession failed after reconnect session=%lu",
               static_cast<unsigned long>(m_client.SessionId()));
    WeaselAppendLogW(L"weasel.tsf.log", L"ipc", buf);
    return false;
  }
  return true;
}

static unsigned int retry = 0;
static DWORD g_last_service_spawn_tick = 0;

bool WeaselTSF::_EnsureServerConnected() {
  if (_server_connected && m_client.Echo())
    return true;
  _server_connected = false;
  if (!m_client.Echo()) {
    _Reconnect();
    retry++;
    const DWORD now = GetTickCount();
    if (retry >= 6 && now - g_last_service_spawn_tick > 60000) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS) {
        g_last_service_spawn_tick = now;
        std::wstring dir = _GetRootDir();
        std::wstring exe = dir + L"\\WeaselServer.exe";
        ShellExecuteW(NULL, L"open", exe.c_str(), NULL, dir.c_str(), SW_HIDE);
        Sleep(300);
        _Reconnect();
      }
      if (hMutex)
        CloseHandle(hMutex);
      retry = 0;
    }
    _server_connected = (m_client.Echo() != 0);
    return _server_connected;
  }
  _server_connected = true;
  return true;
}

namespace {
const wchar_t* kNotifyWndClass = L"WeaselTSFNotifyWnd";

bool IsWeaselServerProcess(DWORD pid) {
  if (!pid)
    return false;
  HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!proc)
    return false;
  wchar_t path[MAX_PATH] = {};
  DWORD size = _countof(path);
  const bool ok =
      QueryFullProcessImageNameW(proc, 0, path, &size) != 0 &&
      [&]() {
        const wchar_t* base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        return _wcsicmp(base, L"WeaselServer.exe") == 0;
      }();
  CloseHandle(proc);
  return ok;
}

LRESULT CALLBACK NotifyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_COPYDATA) {
    auto* self = reinterpret_cast<WeaselTSF*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
    if (!self || !cds || cds->dwData != WEASEL_COMMIT_COPYDATA_ID)
      return FALSE;
    if (!IsWeaselServerProcess(static_cast<DWORD>(wParam)))
      return FALSE;
    if (cds->cbData < sizeof(wchar_t) || !cds->lpData)
      return FALSE;
    const auto* text = reinterpret_cast<const wchar_t*>(cds->lpData);
    const size_t max_chars = cds->cbData / sizeof(wchar_t);
    if (max_chars == 0 || text[max_chars - 1] != L'\0')
      return FALSE;
    const bool ok = self->CommitPhraseFromNotify(std::wstring(text));
    return ok ? TRUE : FALSE;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}  // namespace

BOOL WeaselTSF::_InitNotifyWindow() {
  if (_notify_hwnd)
    return TRUE;
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = NotifyWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = kNotifyWndClass;
  RegisterClassExW(&wc);
  _notify_hwnd = CreateWindowExW(0, kNotifyWndClass, L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, NULL, GetModuleHandle(NULL),
                                 NULL);
  if (!_notify_hwnd)
    return FALSE;
  SetWindowLongPtr(_notify_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  return TRUE;
}

void WeaselTSF::_UninitNotifyWindow() {
  if (_notify_hwnd) {
    DestroyWindow(_notify_hwnd);
    _notify_hwnd = NULL;
  }
}

com_ptr<ITfContext> WeaselTSF::_GetFocusedTfContext() {
  com_ptr<ITfContext> ctx;
  com_ptr<ITfDocumentMgr> doc_mgr;
  if (_pThreadMgr->GetFocus(&doc_mgr) == S_OK && doc_mgr)
    doc_mgr->GetTop(&ctx);
  if (!ctx)
    ctx = _pEditSessionContext;
  return ctx;
}

void WeaselTSF::_RestoreInputFocus() {
  HWND target = _last_focus_hwnd_;
  if (!target || !IsWindow(target))
    target = _GetFocusedContextWindow();
  if (!target)
    return;
  if (GetForegroundWindow() == target)
    return;
  DWORD self_tid = GetCurrentThreadId();
  DWORD target_tid = GetWindowThreadProcessId(target, NULL);
  BOOL attached = FALSE;
  if (target_tid && target_tid != self_tid)
    attached = AttachThreadInput(self_tid, target_tid, TRUE);
  SetForegroundWindow(target);
  SetFocus(target);
  if (attached)
    AttachThreadInput(self_tid, target_tid, FALSE);
}

BOOL WeaselTSF::CommitPhraseFromNotify(const std::wstring& text) {
  if (text.empty())
    return FALSE;
  _RestoreInputFocus();

  auto try_commit = [&](com_ptr<ITfContext> ctx) -> bool {
    return ctx && _CommitTextDirect(ctx, text);
  };

  if (try_commit(_GetFocusedTfContext()))
    return TRUE;

  com_ptr<ITfDocumentMgr> doc_mgr;
  if (_pThreadMgr->GetFocus(&doc_mgr) == S_OK && doc_mgr) {
    com_ptr<ITfContext> top;
    if (doc_mgr->GetTop(&top) == S_OK && try_commit(top))
      return TRUE;
  }

  if (_pEditSessionContext && try_commit(_pEditSessionContext))
    return TRUE;

  if (!_EnsureServerConnected())
    return FALSE;
  m_client.StartSession();
  if (!m_client.CommitText(text)) {
    WeaselAppendLogW(L"weasel.tsf.log", L"phrase",
                     L"CommitPhraseFromNotify CommitText failed");
    return FALSE;
  }
  auto ctx = _GetFocusedTfContext();
  if (!ctx && _pThreadMgr->GetFocus(&doc_mgr) == S_OK && doc_mgr)
    doc_mgr->GetTop(&ctx);
  if (ctx) {
    _pEditSessionContext = ctx;
    _UpdateComposition(ctx);
    return TRUE;
  }
  return FALSE;
}
