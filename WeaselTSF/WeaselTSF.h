#pragma once

#include "Globals.h"
#include <WeaselIPC.h>
#include <WeaselIPCData.h>

class CCandidateList;
class CLangBarItemButton;
class CCompartmentEventSink;

class WeaselTSF : public ITfTextInputProcessorEx,
                  public ITfThreadMgrEventSink,
                  public ITfTextEditSink,
                  public ITfTextLayoutSink,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfThreadFocusSink,
                  public ITfActiveLanguageProfileNotifySink,
                  public ITfEditSession,
                  public ITfDisplayAttributeProvider {
 public:
  WeaselTSF();
  ~WeaselTSF();

  /* IUnknown */
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  /* ITfTextInputProcessor */
  STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId);
  STDMETHODIMP Deactivate();

  /* ITfTextInputProcessorEx */
  STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr,
                          TfClientId tfClientId,
                          DWORD dwFlags);

  /* ITfThreadMgrEventSink */
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr);
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr);
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                          ITfDocumentMgr* pDocMgrPrevFocus);
  STDMETHODIMP OnPushContext(ITfContext* pContext);
  STDMETHODIMP OnPopContext(ITfContext* pContext);

  /* ITfTextEditSink */
  STDMETHODIMP OnEndEdit(ITfContext* pic,
                         TfEditCookie ecReadOnly,
                         ITfEditRecord* pEditRecord);

  /* ITfTextLayoutSink */
  STDMETHODIMP OnLayoutChange(ITfContext* pContext,
                              TfLayoutCode lcode,
                              ITfContextView* pContextView);

  /* ITfKeyEventSink */
  STDMETHODIMP OnSetFocus(BOOL fForeground);
  STDMETHODIMP OnTestKeyDown(ITfContext* pContext,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL* pfEaten);
  STDMETHODIMP OnKeyDown(ITfContext* pContext,
                         WPARAM wParam,
                         LPARAM lParam,
                         BOOL* pfEaten);
  STDMETHODIMP OnTestKeyUp(ITfContext* pContext,
                           WPARAM wParam,
                           LPARAM lParam,
                           BOOL* pfEaten);
  STDMETHODIMP OnKeyUp(ITfContext* pContext,
                       WPARAM wParam,
                       LPARAM lParam,
                       BOOL* pfEaten);
  STDMETHODIMP OnPreservedKey(ITfContext* pContext,
                              REFGUID rguid,
                              BOOL* pfEaten);

  // ITfThreadFocusSink
  STDMETHODIMP OnSetThreadFocus();
  STDMETHODIMP OnKillThreadFocus();

  /* ITfCompositionSink */
  STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite,
                                       ITfComposition* pComposition);

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);
  // 0.18.94: Internal helper for SEHGuard wrapping
  HRESULT DoEditSessionCoreImpl(TfEditCookie ec);

  /* ITfActiveLanguageProfileNotifySink */
  STDMETHODIMP OnActivated(REFCLSID clsid,
                           REFGUID guidProfile,
                           BOOL isActivated);

  // ITfDisplayAttributeProvider
  STDMETHODIMP EnumDisplayAttributeInfo(
      __RPC__deref_out_opt IEnumTfDisplayAttributeInfo** ppEnum);
  STDMETHODIMP GetDisplayAttributeInfo(
      __RPC__in REFGUID guidInfo,
      __RPC__deref_out_opt ITfDisplayAttributeInfo** ppInfo);

  ///* ITfCompartmentEventSink */
  // STDMETHODIMP OnChange(_In_ REFGUID guid);

  /* Compartments */
  BOOL _IsKeyboardDisabled();
  BOOL _IsKeyboardOpen();
  HRESULT _SetKeyboardOpen(BOOL fOpen);
  HRESULT _GetCompartmentDWORD(DWORD& value, const GUID guid);
  HRESULT _SetCompartmentDWORD(const DWORD& value, const GUID guid);

  /* Composition */
  void _StartComposition(com_ptr<ITfContext> pContext,
                         BOOL fCUASWorkaroundEnabled);
  void _EndComposition(com_ptr<ITfContext> pContext, BOOL clear);
  void _EndCompositionInSession(TfEditCookie ec,
                                com_ptr<ITfContext> pContext,
                                BOOL clear);
  void _QueueEndCompositionEditSession(com_ptr<ITfContext> pContext, BOOL clear);
  void _SyncEndCompositionOnIdle(com_ptr<ITfContext> pContext);
  void _ResetStaleCompositionState();
  void _EnterEditSession();
  void _LeaveEditSession();
  void _FlushDeferredCompositionActions();
  BOOL _ShowInlinePreedit(com_ptr<ITfContext> pContext,
                          const std::shared_ptr<weasel::Context> context);
  void _UpdateComposition(com_ptr<ITfContext> pContext, BOOL sync = FALSE);
  BOOL _IsComposing() const;
  bool _ShouldIsolateShiftForComposing() const;
  void _SetComposition(com_ptr<ITfComposition> pComposition);
  void _SetCompositionPosition(const RECT& rc);
  BOOL _UpdateCompositionWindow(com_ptr<ITfContext> pContext);
  void _FinalizeComposition();
  void _AbortComposition(bool clear = true, bool force = false);
  UINT _CompositionGeneration() const { return _composition_generation; }
  BOOL _IsActiveComposition(ITfComposition* comp) const {
    return comp && _pComposition && _pComposition.p == comp;
  }
  void _ClearEndCompositionPending() { _end_composition_pending = FALSE; }

  /* Language bar */
  HWND _GetFocusedContextWindow();
  void _HandleLangBarMenuSelect(UINT wID);
  void ShowQuickBar(POINT pt);
  BOOL CommitPhraseFromNotify(const std::wstring& text);
  BOOL _CommitTextDirect(com_ptr<ITfContext> pContext, const std::wstring& text);
  com_ptr<ITfContext> _GetFocusedTfContext();
  void _RestoreInputFocus();
  void _ToggleAsciiMode(BOOL lock_rime = FALSE);
  bool _RefreshStatusFromResponse();
  void SyncLanguageBarTrayVisibility() { _SyncLanguageBarTrayVisibility(); }
  bool _HasCandidateMenu();
  bool _CanDeleteCandidate();
  bool _ShouldAllowShiftCandidatePick(WPARAM wParam,
                                      LPARAM lParam,
                                      bool key_up) const;
  bool _HasVisibleCandidateMenu() const;

  bool _EnsureIpcSession();
  void _EnsureStyleFromSession();
  /* IPC */
  bool _EnsureServerConnected();

  /* UI */
  void _UpdateUI(const weasel::Context& ctx, const weasel::Status& status);
  void _StartUI();
  void _EndUI();
  void _ShowUI();
  void _HideUI();
  com_ptr<ITfContext> _GetUIContextDocument();

  /* Display Attribute */
  void _ClearCompositionDisplayAttributes(TfEditCookie ec,
                                          _In_ ITfContext* pContext);
  BOOL _SetCompositionDisplayAttributes(TfEditCookie ec,
                                        _In_ ITfContext* pContext,
                                        ITfRange* pRangeComposition);
  BOOL _InitDisplayAttributeGuidAtom();

  com_ptr<ITfThreadMgr> _GetThreadMgr() { return _pThreadMgr; }
  void HandleUICallback(size_t* const sel,
                        size_t* const hov,
                        bool* const next,
                        bool* const scroll_next);
  void _DeleteCandidateOnCurrentPage(const size_t index);

 private:
  /* ui callback functions private */
  void _SelectCandidateOnCurrentPage(const size_t index);
  void _HandleMouseHoverEvent(const size_t index);
  void _HandleMousePageEvent(bool* const nextPage, bool* const scrollNextPage);
  /* TSF Related */
  BOOL _InitThreadMgrEventSink();
  void _UninitThreadMgrEventSink();
  // ITfThreadFocusSink
  BOOL _InitThreadFocusSink();
  void _UninitThreadFocusSink();
  DWORD _dwThreadFocusSinkCookie;

  BOOL _InitTextEditSink(com_ptr<ITfDocumentMgr> pDocMgr);

  BOOL _InitKeyEventSink();
  void _UninitKeyEventSink();
  void _ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  void _ProcessKeyEventImpl(WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
  bool _PullResponseAfterKey();
  void _AfterRimeKey(com_ptr<ITfContext> pContext, BOOL eaten);
  void _KeySinkReset();
  bool _HandleIdleKeyResponse(com_ptr<ITfContext> pContext);
  void _RequestKeyEditSession(com_ptr<ITfContext> pContext, bool has_commit);
  bool _CompositionStaleMismatch() const;

  BOOL _InitPreservedKey();
  void _UninitPreservedKey();

  BOOL _InitLanguageBar();
  void _UninitLanguageBar();
  void _UpdateLanguageBar(weasel::Status stat, bool force_compartment_sync = false);
  void _ShowLanguageBar(BOOL show);
  void _SyncLanguageBarTrayVisibility();
  void _EnableLanguageBar(BOOL enable);

  BOOL _InsertText(com_ptr<ITfContext> pContext, const std::wstring& ext);

  void _DeleteCandidateList();

  BOOL _InitCompartment();
  void _UninitCompartment();
  HRESULT _HandleCompartment(REFGUID guidCompartment);
  void _ScheduleTrayCommand(UINT wID);
  void _FlushPendingTrayCommand();

  void _Reconnect();
  std::wstring _GetRootDir();

  bool isImmersive() const {
    return (_activateFlags & TF_TMF_IMMERSIVEMODE) != 0;
  }

  com_ptr<ITfThreadMgr> _pThreadMgr;
  TfClientId _tfClientId;
  DWORD _dwThreadMgrEventSinkCookie;

  com_ptr<ITfContext> _pTextEditSinkContext;
  DWORD _dwTextEditSinkCookie, _dwTextLayoutSinkCookie;
  BYTE _lpbKeyState[256];

  struct KeySinkState {
    BOOL down_pending = FALSE;
    BOOL up_pending = FALSE;
    BOOL eaten = FALSE;
    BOOL down_seen = FALSE;
    BOOL up_seen = FALSE;
    BOOL down_rime = FALSE;
    BOOL up_rime = FALSE;
    BOOL down_after_rime = FALSE;

    bool AnyTestPending() const { return down_pending || up_pending; }

    void ResetDown() {
      down_pending = FALSE;
      down_seen = FALSE;
      down_rime = FALSE;
      down_after_rime = FALSE;
    }

    void ResetUp() {
      up_pending = FALSE;
      up_seen = FALSE;
      up_rime = FALSE;
    }

    void ResetAll() {
      ResetDown();
      ResetUp();
    }
  } _key_sink;
  BOOL _rime_key_dispatched = FALSE;
  BOOL _ipc_refresh_requested = FALSE;
  BOOL _ipc_refresh_urgent = FALSE;
  BOOL _ipc_refresh_force = FALSE;

  void _RequestIpcRefresh(BOOL urgent = FALSE, BOOL force = FALSE);

  com_ptr<ITfContext> _pEditSessionContext;
  std::wstring _editSessionText;

  com_ptr<CCompartmentEventSink> _pKeyboardCompartmentSink;
  com_ptr<CCompartmentEventSink> _pConvertionCompartmentSink;

  com_ptr<ITfComposition> _pComposition;

  com_ptr<CLangBarItemButton> _pLangBarButton;

  com_ptr<CCandidateList> _cand;

  LONG _cRef;  // COM ref count

  /* CUAS Candidate Window Position Workaround */
  BOOL _fCUASWorkaroundTested, _fCUASWorkaroundEnabled;

  /* Weasel Related */
  weasel::Client m_client;
  DWORD _activateFlags;

  /* IME status */
  weasel::Status _status;

  // guidatom for the display attibute.
  TfGuidAtom _gaDisplayAttributeInput;
  BOOL _async_edit = false;
  UINT _composition_generation = 0;
  UINT _edit_session_depth = 0;
  BOOL _end_composition_pending = FALSE;
  BOOL _edit_session_pending = FALSE;
  BOOL _composition_dirty = FALSE;
  struct {
    BOOL end_requested = FALSE;
    BOOL end_clear = TRUE;
    com_ptr<ITfContext> end_context;
    BOOL layout_update = FALSE;
  } _deferred;
  BOOL _committed = false;
  BOOL _isToOpenClose = false;
  BOOL _isSyncingCompartment = FALSE;
  UINT _pendingTrayCommand = 0;
  HWND _notify_hwnd = NULL;
  HWND _last_focus_hwnd_ = NULL;
  bool _server_connected = false;
  DWORD _ascii_toggle_tick_ = 0;
  BOOL _ascii_toggle_value_ = FALSE;
  // 0.18.97: 标记本次 toggle 是否锁定 RIME 引擎状态。
  // 仅当 shift 单独释放触发 _ToggleAsciiMode 时才为 true，
  // 防止后续 RIME key 处理"反方向"切换覆盖 toggle 结果；
  // shift+ctrl+其他按键 等组合场景下不锁定，不影响 RIME 正常处理。
  BOOL _ascii_toggle_lock_rime_ = FALSE;

  struct KeyPullCache {
    BOOL valid = FALSE;
    BOOL ok = FALSE;
    std::wstring commit;
    std::shared_ptr<weasel::Context> context;
    weasel::Config config;
    weasel::UIStyle style;
  } _key_pull;

  BOOL _InitNotifyWindow();
  void _UninitNotifyWindow();
};
