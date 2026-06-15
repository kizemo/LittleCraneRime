#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"
#include "ResponseParser.h"
#include "CandidateList.h"
#include <WeaselEditSessionSafe.h>
#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>
#include <WeaselTSFCallbackSafe.h>

namespace {

class EditSessionDepthGuard {
 public:
  explicit EditSessionDepthGuard(WeaselTSF* tsf) : tsf_(tsf) {
    tsf_->_EnterEditSession();
  }
  ~EditSessionDepthGuard() { tsf_->_LeaveEditSession(); }

 private:
  WeaselTSF* tsf_;
};

}  // namespace

/* Start Composition */
class CStartCompositionEditSession : public CEditSession {
 public:
  CStartCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                               com_ptr<ITfContext> pContext,
                               BOOL fCUASWorkaroundEnabled,
                               BOOL inlinePreeditEnabled)
      : CEditSession(pTextService, pContext),
        _inlinePreeditEnabled(inlinePreeditEnabled) {
    _fCUASWorkaroundEnabled = fCUASWorkaroundEnabled;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  BOOL _fCUASWorkaroundEnabled;
  BOOL _inlinePreeditEnabled;
};

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec) {
  return weasel::RunEditSessionSafe(L"CStartCompositionEditSession", [&]() -> HRESULT {
  EditSessionDepthGuard guard(_pTextService);
  HRESULT hr = E_FAIL;
  if (!_pContext)
    return E_FAIL;
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  if (_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                (LPVOID*)&pInsertAtSelection) != S_OK)
    return hr;
  if (pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0,
                                                &pRangeComposition) != S_OK)
    return hr;

  com_ptr<ITfContextComposition> pContextComposition;
  com_ptr<ITfComposition> pComposition;
  if (_pContext->QueryInterface(IID_ITfContextComposition,
                                (LPVOID*)&pContextComposition) != S_OK)
    return hr;
  if ((pContextComposition->StartComposition(
           ec, pRangeComposition, _pTextService, &pComposition) == S_OK) &&
      (pComposition != NULL)) {
    _pTextService->_SetComposition(pComposition);

    /* WORKAROUND:
     *   CUAS does not provide a correct GetTextExt() position unless the
     * composition is filled with characters. So we insert a zero width space
     * here. The workaround is only needed when inline preedit is not enabled.
     *   See https://github.com/rime/weasel/pull/883#issuecomment-1567625762
     */
    if (!_inlinePreeditEnabled) {
      if (pRangeComposition)
        pRangeComposition->SetText(ec, TF_ST_CORRECTION, L" ", 1);
    }

    /* set selection */
    TF_SELECTION tfSelection;
    if (_inlinePreeditEnabled) {
      if (pRangeComposition)
        pRangeComposition->Collapse(ec, TF_ANCHOR_END);
    } else {
      if (pRangeComposition)
        pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    }
    if (pRangeComposition) {
      tfSelection.range = pRangeComposition;
      tfSelection.style.ase = TF_AE_NONE;
      tfSelection.style.fInterimChar = FALSE;
      _pContext->SetSelection(ec, 1, &tfSelection);
    }
  }

  return hr;
  });
}

void WeaselTSF::_StartComposition(com_ptr<ITfContext> pContext,
                                  BOOL fCUASWorkaroundEnabled) {
  if (!pContext)
    return;
  com_ptr<CStartCompositionEditSession> pStartCompositionEditSession;
  pStartCompositionEditSession.Attach(new CStartCompositionEditSession(
      this, pContext, fCUASWorkaroundEnabled, _cand->style().inline_preedit));
  _cand->StartUI();
  if (pStartCompositionEditSession != nullptr) {
    HRESULT hr;
    try {
      pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession,
                                   TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    } catch (...) {
      WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                       L"exception in _StartComposition RequestEditSession");
      weasel_crash_diag::Crumb(L"composition", L"start request ex");
    }
  }
}

/* End Composition */
class CEndCompositionEditSession : public CEditSession {
 public:
  CEndCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                             com_ptr<ITfContext> pContext,
                             com_ptr<ITfComposition> pComposition,
                             BOOL clear = TRUE)
      : CEditSession(pTextService, pContext),
        _clear(clear),
        _generation(pTextService->_CompositionGeneration()) {
    _pComposition = pComposition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  BOOL _clear;
  UINT _generation;
};

STDAPI CEndCompositionEditSession::DoEditSession(TfEditCookie ec) {
  return weasel::RunEditSessionSafe(L"CEndCompositionEditSession", [&]() -> HRESULT {
  EditSessionDepthGuard guard(_pTextService);
  _pTextService->_ClearEndCompositionPending();
  if (_pComposition == nullptr)
    return S_OK;
  if (_generation != _pTextService->_CompositionGeneration() ||
      !_pTextService->_IsActiveComposition(_pComposition))
    return S_OK;

  if (_clear)
    _pTextService->_EndUI();
  _pTextService->_EndCompositionInSession(ec, _pContext, _clear);
  return S_OK;
  });
}

void WeaselTSF::_EndCompositionInSession(TfEditCookie ec,
                                         com_ptr<ITfContext> pContext,
                                         BOOL clear) {
  if (!_pComposition)
    return;

  _ClearCompositionDisplayAttributes(ec, pContext);

  ITfRange* pCompositionRange;
  if (clear && _pComposition->GetRange(&pCompositionRange) == S_OK)
    pCompositionRange->SetText(ec, 0, L"", 0);

  _pComposition->EndComposition(ec);
  _FinalizeComposition();
}

void WeaselTSF::_QueueEndCompositionEditSession(com_ptr<ITfContext> pContext,
                                                BOOL clear) {
  CEndCompositionEditSession* pEditSession;
  HRESULT hr;

  if (!pContext || !_pComposition || _end_composition_pending)
    return;

  if (clear)
    _cand->EndUI();
  if ((pEditSession = new CEndCompositionEditSession(
           this, pContext, _pComposition, clear)) != NULL) {
    _end_composition_pending = TRUE;
    try {
      pContext->RequestEditSession(_tfClientId, pEditSession,
                                   TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    } catch (...) {
      WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                       L"exception in _QueueEndComposition RequestEditSession");
      weasel_crash_diag::Crumb(L"composition", L"end request ex");
      hr = E_FAIL;
    }
    pEditSession->Release();
    if (FAILED(hr))
      _end_composition_pending = FALSE;
  }
}

void WeaselTSF::_EndComposition(com_ptr<ITfContext> pContext, BOOL clear) {
  if (!_pComposition || _end_composition_pending)
    return;

  if (_edit_session_depth > 0) {
    _deferred.end_requested = TRUE;
    _deferred.end_clear = clear;
    _deferred.end_context = pContext;
    return;
  }

  _QueueEndCompositionEditSession(pContext, clear);
}

/* Get Text Extent */
class CGetTextExtentEditSession : public CEditSession {
 public:
  CGetTextExtentEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfContextView> pContextView,
                            com_ptr<ITfComposition> pComposition,
                            bool enhancedPosition)
      : CEditSession(pTextService, pContext) {
    _pContextView = pContextView;
    _pComposition = pComposition;
    _enhancedPosition = enhancedPosition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfContextView> _pContextView;
  com_ptr<ITfComposition> _pComposition;
  bool _enhancedPosition;
};

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec) {
  return weasel::RunEditSessionSafe(L"CGetTextExtentEditSession", [&]() -> HRESULT {
  EditSessionDepthGuard guard(_pTextService);
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  ITfRange* pRange;
  RECT rc;
  BOOL fClipped;
  TF_SELECTION selection;
  ULONG nSelection;

  if (FAILED(_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                       (LPVOID*)&pInsertAtSelection)))
    return E_FAIL;
  if (FAILED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection,
                                     &nSelection)))
    return E_FAIL;

  if (_pComposition != nullptr && _pComposition->GetRange(&pRange) == S_OK) {
    pRange->Collapse(ec, TF_ANCHOR_START);
  } else {
    // composition end
    // note: selection.range is always an empty range
    pRange = selection.range;
  }

  BOOL has_valid_rect = FALSE;
  if ((_pContextView->GetTextExt(ec, pRange, &rc, &fClipped)) == S_OK &&
      (rc.left != 0 || rc.top != 0)) {
    has_valid_rect = TRUE;
  }
  if (!has_valid_rect) {
    HWND hwnd = _pTextService->_GetFocusedContextWindow();
    if (!hwnd)
      hwnd = GetForegroundWindow();
    POINT pt = {0, 0};
    BOOL got_caret = FALSE;
    GUITHREADINFO gti = {sizeof(gti)};
    if (GetGUIThreadInfo(GetWindowThreadProcessId(hwnd, NULL), &gti) &&
        gti.hwndCaret) {
      pt.x = gti.rcCaret.left;
      pt.y = gti.rcCaret.bottom;
      ClientToScreen(gti.hwndCaret, &pt);
      got_caret = TRUE;
    }
    if (!got_caret && hwnd && GetCaretPos(&pt))
      ClientToScreen(hwnd, &pt);
    if (!got_caret && !hwnd)
      GetCursorPos(&pt);
    rc.left = pt.x;
    rc.top = pt.y;
    rc.right = pt.x + 8;
    rc.bottom = pt.y + 20;
    has_valid_rect = TRUE;
  }
  if (has_valid_rect) {
    // get the foreground window pos and check if rc from GetTextExt is out of
    // window
    if (_enhancedPosition) {
      HWND hwnd;
      RECT rcForegroundWindow;
      hwnd = GetForegroundWindow();
      ::GetWindowRect(hwnd, &rcForegroundWindow);

      if (rc.left < rcForegroundWindow.left ||
          rc.left > rcForegroundWindow.right ||
          rc.top < rcForegroundWindow.top ||
          rc.top > rcForegroundWindow.bottom) {
        POINT pt;
        bool hasCaret = ::GetCaretPos(&pt);
        int offsetx = rcForegroundWindow.left - rc.left + (hasCaret ? pt.x : 0);
        int offsety = rcForegroundWindow.top - rc.top + (hasCaret ? pt.y : 0);
        rc.left += offsetx;
        rc.right += offsetx;
        rc.top += offsety;
        rc.bottom += offsety;
      }
    }
    _pTextService->_SetCompositionPosition(rc);
  }
  return S_OK;
  });
}

/* Composition Window Handling */
BOOL WeaselTSF::_UpdateCompositionWindow(com_ptr<ITfContext> pContext) {
  com_ptr<ITfContextView> pContextView;
  if (!pContext)
    return FALSE;
  if (pContext->GetActiveView(&pContextView) != S_OK)
    return FALSE;
  com_ptr<CGetTextExtentEditSession> pEditSession;
  pEditSession.Attach(
      new CGetTextExtentEditSession(this, pContext, pContextView, _pComposition,
                                    _cand->style().enhanced_position));
  if (pEditSession == NULL) {
    return FALSE;
  }
  HRESULT hr;
  try {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READ, &hr);
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                     L"exception in _UpdateCompositionWindow RequestEditSession");
    weasel_crash_diag::Crumb(L"composition", L"window request ex");
    return FALSE;
  }
  return SUCCEEDED(hr);
}

void WeaselTSF::_SetCompositionPosition(const RECT& rc) {
  /* Test if rect is valid.
   * If it is invalid during CUAS test, we need to apply CUAS workaround */
  if (!_fCUASWorkaroundTested) {
    _fCUASWorkaroundTested = TRUE;
    if (rc.top == rc.bottom) {
      _fCUASWorkaroundEnabled = TRUE;
      return;
    }
  }
  RECT _rc;
  _rc.left = _rc.right = rc.left;
  _rc.top = _rc.bottom = rc.bottom;
  m_client.UpdateInputPosition(rc);
  _cand->UpdateInputPosition(rc);
}

/* Inline Preedit */
class CInlinePreeditEditSession : public CEditSession {
 public:
  CInlinePreeditEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfComposition> pComposition,
                            const std::shared_ptr<weasel::Context> context)
      : CEditSession(pTextService, pContext),
        _pComposition(pComposition),
        _context(context) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  const std::shared_ptr<weasel::Context> _context;
};

STDAPI CInlinePreeditEditSession::DoEditSession(TfEditCookie ec) {
  return weasel::RunEditSessionSafe(L"CInlinePreeditEditSession", [&]() -> HRESULT {
  EditSessionDepthGuard guard(_pTextService);
  std::wstring preedit = _context->preedit.str;

  com_ptr<ITfRange> pRangeComposition;
  if (_pComposition == nullptr)
    return E_FAIL;
  if ((_pComposition->GetRange(&pRangeComposition)) != S_OK)
    return E_FAIL;

  if ((pRangeComposition->SetText(ec, 0, preedit.c_str(),
                                  static_cast<LONG>(preedit.length()))) != S_OK)
    return E_FAIL;

  /* TODO: Check the availability and correctness of these values */
  int sel_cursor = -1;
  for (size_t i = 0; i < _context->preedit.attributes.size(); i++) {
    if (_context->preedit.attributes.at(i).type == weasel::HIGHLIGHTED) {
      sel_cursor = _context->preedit.attributes.at(i).range.cursor;
      break;
    }
  }

  _pTextService->_SetCompositionDisplayAttributes(ec, _pContext,
                                                  pRangeComposition);

  /* Set caret */
  LONG cch;
  TF_SELECTION tfSelection;
  if (sel_cursor < 0) {
    pRangeComposition->Collapse(ec, TF_ANCHOR_END);
  } else {
    pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    pRangeComposition->ShiftStart(ec, sel_cursor, &cch, NULL);
  }
  tfSelection.range = pRangeComposition;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;
  _pContext->SetSelection(ec, 1, &tfSelection);

  return S_OK;
  });
}

BOOL WeaselTSF::_ShowInlinePreedit(
    com_ptr<ITfContext> pContext,
    const std::shared_ptr<weasel::Context> context) {
  com_ptr<CInlinePreeditEditSession> pEditSession;
  pEditSession.Attach(
      new CInlinePreeditEditSession(this, pContext, _pComposition, context));
  if (pEditSession != NULL) {
    HRESULT hr;
    try {
      pContext->RequestEditSession(_tfClientId, pEditSession,
                                   TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    } catch (...) {
      WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                       L"exception in _ShowInlinePreedit RequestEditSession");
      weasel_crash_diag::Crumb(L"composition", L"preedit request ex");
    }
  }
  return TRUE;
}

/* Update Composition */
class CInsertTextEditSession : public CEditSession {
 public:
  CInsertTextEditSession(com_ptr<WeaselTSF> pTextService,
                         com_ptr<ITfContext> pContext,
                         com_ptr<ITfComposition> pComposition,
                         const std::wstring& text)
      : CEditSession(pTextService, pContext),
        _text(text),
        _pComposition(pComposition) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  std::wstring _text;
  com_ptr<ITfComposition> _pComposition;
};

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec) {
  return weasel::RunEditSessionSafe(L"CInsertTextEditSession", [&]() -> HRESULT {
  EditSessionDepthGuard guard(_pTextService);
  com_ptr<ITfRange> pRange;
  TF_SELECTION tfSelection;
  HRESULT hRet = S_OK;

  if (!_pContext || _pComposition == nullptr)
    return E_FAIL;
  if (FAILED(_pComposition->GetRange(&pRange)))
    return E_FAIL;
  if (!pRange)
    return E_FAIL;

  if (FAILED(pRange->SetText(ec, 0, _text.c_str(),
                             static_cast<LONG>(_text.length()))))
    return E_FAIL;

  /* update the selection to an insertion point just past the inserted text. */
  pRange->Collapse(ec, TF_ANCHOR_END);

  tfSelection.range = pRange;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;

  _pContext->SetSelection(ec, 1, &tfSelection);

  return hRet;
  });
}

class CCommitTextDirectEditSession : public CEditSession {
 public:
  CCommitTextDirectEditSession(com_ptr<WeaselTSF> pTextService,
                               com_ptr<ITfContext> pContext,
                               const std::wstring& text)
      : CEditSession(pTextService, pContext), _text(text) {}

  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  std::wstring _text;
};

STDMETHODIMP CCommitTextDirectEditSession::DoEditSession(TfEditCookie ec) {
  try {
    EditSessionDepthGuard guard(_pTextService);
    com_ptr<ITfInsertAtSelection> insert;
    if (_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                  (void**)&insert) != S_OK)
      return E_FAIL;
    com_ptr<ITfRange> range;
    LONG len = static_cast<LONG>(_text.size());
    if (FAILED(insert->InsertTextAtSelection(ec, TF_IAS_NO_DEFAULT_COMPOSITION,
                                           _text.c_str(), len, &range)))
      return E_FAIL;
    TF_SELECTION sel = {};
    sel.range = range;
    sel.range->Collapse(ec, TF_ANCHOR_END);
    sel.style.ase = TF_AE_NONE;
    sel.style.fInterimChar = FALSE;
    _pContext->SetSelection(ec, 1, &sel);
    return S_OK;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"commit",
                     L"exception in CommitTextDirect edit session");
    return E_FAIL;
  }
}

BOOL WeaselTSF::_CommitTextDirect(com_ptr<ITfContext> pContext,
                                  const std::wstring& text) {
  if (!pContext || text.empty())
    return FALSE;
  CCommitTextDirectEditSession* session =
      new CCommitTextDirectEditSession(this, pContext, text);
  HRESULT hr = S_OK;
  try {
    pContext->RequestEditSession(_tfClientId, session,
                                 TF_ES_SYNC | TF_ES_READWRITE, &hr);
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                     L"exception in _CommitTextDirect RequestEditSession");
    weasel_crash_diag::Crumb(L"composition", L"commit-direct request ex");
    hr = E_FAIL;
  }
  session->Release();
  return SUCCEEDED(hr);
}

BOOL WeaselTSF::_InsertText(com_ptr<ITfContext> pContext,
                            const std::wstring& text) {
  CInsertTextEditSession* pEditSession;
  HRESULT hr;

  if (!pContext)
    return FALSE;
  if ((pEditSession = new CInsertTextEditSession(this, pContext, _pComposition,
                                                 text)) != NULL) {
    try {
      pContext->RequestEditSession(_tfClientId, pEditSession,
                                   TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    } catch (...) {
      WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                       L"exception in _InsertText RequestEditSession");
      weasel_crash_diag::Crumb(L"composition", L"insert request ex");
    }
    pEditSession->Release();
  }

  return TRUE;
}

void WeaselTSF::_UpdateComposition(com_ptr<ITfContext> pContext, BOOL sync) {
  HRESULT hr;

  if (!pContext) {
    _composition_dirty = TRUE;
    return;
  }
  _pEditSessionContext = pContext;

  if (sync) {
    _ipc_refresh_requested = TRUE;
    if (_edit_session_depth > 0) {
      _composition_dirty = TRUE;
      return;
    }
  }

  if (_edit_session_pending && !sync) {
    _composition_dirty = TRUE;
    return;
  }
  if (sync)
    _edit_session_pending = FALSE;

  const DWORD flags =
      (sync ? TF_ES_SYNC : TF_ES_ASYNCDONTCARE) | TF_ES_READWRITE;
  try {
    _pEditSessionContext->RequestEditSession(_tfClientId, this, flags, &hr);
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                     L"exception in _UpdateComposition RequestEditSession");
    weasel_crash_diag::Crumb(L"composition", L"update request ex");
    _edit_session_pending = FALSE;
    hr = E_FAIL;
  }
  _async_edit = !!(hr == TF_S_ASYNC);
  // Only mark pending when the session is actually queued. S_OK means DoEditSession
  // already ran synchronously; setting pending there would drop all later input.
  if (!sync && hr == TF_S_ASYNC)
    _edit_session_pending = TRUE;
}

/* Composition State */
STDAPI WeaselTSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  return weasel_tsf_safe::RunTSFCallback(L"OnCompositionTerminated",
                                          [&]() -> HRESULT {
  try {
    // NOTE:
    // This will be called when an edit session ended up with an empty composition
    // string, Even if it is closed normally. Silly M$.
    // During partial commit (long sentence), Rime is still composing while TSF
    // ends the old composition range — do not tear down UI / clear Rime session.
    if (_status.composing) {
      _FinalizeComposition();
      return S_OK;
    }

    _HideUI();
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                     L"uncaught exception in OnCompositionTerminated");
    weasel_crash_diag::Crumb(L"composition", L"OnCompositionTerminated ex");
  }
  return S_OK;
  });
}

void WeaselTSF::_AbortComposition(bool clear, bool force) {
  if (!force && _key_sink.AnyTestPending())
    return;
  m_client.ClearComposition();
  if (_IsComposing()) {
    _EndComposition(_pEditSessionContext, clear);
  } else {
    _HideUI();
  }
  _committed = TRUE;
}

void WeaselTSF::_FinalizeComposition() {
  _pComposition = nullptr;
  ++_composition_generation;
  _end_composition_pending = FALSE;
}

void WeaselTSF::_EnterEditSession() {
  ++_edit_session_depth;
}

void WeaselTSF::_LeaveEditSession() {
  if (_edit_session_depth > 0)
    --_edit_session_depth;
  if (_edit_session_depth == 0)
    _FlushDeferredCompositionActions();
}

void WeaselTSF::_FlushDeferredCompositionActions() {
  if (_deferred.end_requested && _deferred.end_context) {
    com_ptr<ITfContext> ctx = _deferred.end_context;
    const BOOL clear = _deferred.end_clear;
    _deferred.end_requested = FALSE;
    _deferred.end_context = nullptr;
    if (_pComposition && !_end_composition_pending)
      _QueueEndCompositionEditSession(ctx, clear);
  }
  if (_deferred.layout_update && _pEditSessionContext &&
      (_IsComposing() || _status.composing)) {
    _deferred.layout_update = FALSE;
    _UpdateCompositionWindow(_pEditSessionContext);
  }
  if (_composition_dirty && _pEditSessionContext && !_edit_session_pending) {
    if (_ipc_refresh_requested) {
      _composition_dirty = FALSE;
      _UpdateComposition(_pEditSessionContext, FALSE);
    }
  }
}

void WeaselTSF::_SetComposition(com_ptr<ITfComposition> pComposition) {
  _pComposition = pComposition;
  ++_composition_generation;
  _end_composition_pending = FALSE;
}

BOOL WeaselTSF::_IsComposing() const {
  return _pComposition != NULL;
}

bool WeaselTSF::_ShouldIsolateShiftForComposing() const {
  if (_status.ascii_mode)
    return false;
  if (_status.composing)
    return true;
  // Stale IPC status fallback: visible candidate menu (>=2) implies active compose.
  UINT cand_count = 0;
  if (_cand && _cand->GetCount(&cand_count) == S_OK && cand_count >= 2)
    return true;
  return false;
}

bool WeaselTSF::_PullResponseAfterKey() {
  _key_pull.valid = FALSE;
  _key_pull.ok = FALSE;
  _key_pull.commit.clear();
  _key_pull.context = std::make_shared<weasel::Context>();
  _key_pull.config = weasel::Config();
  weasel::ResponseParser parser(&_key_pull.commit, _key_pull.context.get(),
                                &_status, &_key_pull.config, &_cand->style());
  try {
    _key_pull.ok = m_client.GetResponseData(std::ref(parser)) ? TRUE : FALSE;
    _key_pull.valid = TRUE;
    _key_pull.style = _cand->style();
    return _key_pull.ok != FALSE;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"key-ipc", L"GetResponseData exception");
    weasel_crash_diag::Crumb(L"key-ipc", L"exception");
    return false;
  }
}

void WeaselTSF::_SyncEndCompositionOnIdle(com_ptr<ITfContext> pContext) {
  if (!_pComposition || !pContext || _edit_session_depth > 0)
    return;
  if (_end_composition_pending) {
    _end_composition_pending = FALSE;
    _FinalizeComposition();
    if (!_pComposition)
      return;
  }
  _cand->EndUI();
  CEndCompositionEditSession* pEditSession = new CEndCompositionEditSession(
      this, pContext, _pComposition, FALSE);
  if (!pEditSession)
    return;
  _end_composition_pending = TRUE;
  HRESULT hr = E_FAIL;
  try {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_SYNC | TF_ES_READWRITE, &hr);
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"composition",
                     L"exception in _SyncEndCompositionOnIdle RequestEditSession");
    weasel_crash_diag::Crumb(L"composition", L"sync-end request ex");
    hr = E_FAIL;
  }
  pEditSession->Release();
  if (FAILED(hr)) {
    _end_composition_pending = FALSE;
    _FinalizeComposition();
  }
}

void WeaselTSF::_ResetStaleCompositionState() {
  const bool stale_pending =
      _end_composition_pending && _edit_session_depth == 0 &&
      (!_edit_session_pending || !_status.composing);
  const bool stale_composition =
      _IsComposing() && !_status.composing && _edit_session_depth == 0;

  if (!stale_pending && !stale_composition)
    return;

  _end_composition_pending = FALSE;
  _edit_session_pending = FALSE;
  _composition_dirty = FALSE;
  _deferred.end_requested = FALSE;
  _deferred.end_context = nullptr;
  _deferred.layout_update = FALSE;
  if (_IsComposing())
    _FinalizeComposition();
  weasel_crash_diag::Crumb(L"compose", stale_pending
                                           ? L"reset stale pending"
                                           : L"reset stale composition");
}

void WeaselTSF::_AfterRimeKey(com_ptr<ITfContext> pContext, BOOL eaten) {
  _ResetStaleCompositionState();
  if (_cand->style().font_point <= 0)
    _EnsureStyleFromSession();
  if (!_PullResponseAfterKey())
    return;

  const bool has_commit = !_key_pull.commit.empty();
  const bool idle = !has_commit && !_status.composing &&
                    _key_pull.context->preedit.str.empty() &&
                    _key_pull.context->cinfo.candies.empty();

  // 0.18.97: 恢复 force-sync，但仅在 _ascii_toggle_lock_rime_=true 时启用。
  // 之前 0.18.96 移除该逻辑后导致 shift 单独释放的 toggle 被 RIME 引擎的
  // "反方向"切换覆盖，状态在 1.5s 内被反转。
  // 现在仅在 shift 单独释放触发的 toggle（_ToggleAsciiMode(TRUE)）时锁定 RIME ；
  // shift+ctrl+其他按键 等组合场景下不会进入 _ToggleAsciiMode，因此 lock_rime=false，
  // force-sync 不生效，RIME 正常处理按键。
  //
  // 0.18.99.1: BUG 修复 - 不再在第一次确认匹配时立即清空 lock。
  // 原逻辑：第一次 key 确认匹配 (ascii_mode == toggle_value) 时立刻
  //   _ascii_toggle_tick_=0; _ascii_toggle_lock_rime_=FALSE;
  // 问题：1.5s 内的第二次 key 若被 RIME schema 翻回，lock 已失效，force-sync 不再生效，
  //   导致"先切英文、再按键又回到中文"现象。
  // 现逻辑：lock 仅在以下条件之一满足时释放：
  //   1. 锁定时长已到 (>1500ms)，并且当前模式与目标一致（自然确认）→ 释放
  //   2. 用户再次主动切换（_ToggleAsciiMode 会重新设置 tick 与 value）
  //   3. 进程级 reset（focus 切换/_Reconnect）已经在 _Reconnect/_KeySinkReset 内清空
  // 这样锁在 1.5s 窗口内持续生效，可稳定覆盖最多一个"反弹"按键。
  BOOL langbar_force = FALSE;
  const DWORD now_tick = GetTickCount();
  const bool within_lock_window =
      _ascii_toggle_tick_ && (now_tick - _ascii_toggle_tick_) < 1500;
  if (_ascii_toggle_lock_rime_ &&
      _ascii_toggle_tick_ &&
      !!_status.ascii_mode != !!_ascii_toggle_value_) {
    // RIME 状态与用户选择的 toggle 不一致 → 强制同步
    _status.ascii_mode = !!_ascii_toggle_value_;
    _status.full_shape = !!_ascii_toggle_value_;
    langbar_force = TRUE;
  } else if (_ascii_toggle_tick_ && !within_lock_window &&
             !!_status.ascii_mode == !!_ascii_toggle_value_) {
    // 锁窗口外，且模式已稳定为目标值 → 释放 lock
    _ascii_toggle_tick_ = 0;
    _ascii_toggle_lock_rime_ = FALSE;
  }
  // 锁窗口内的确认匹配：保留 lock，不重置 tick（防止 1.5s 内下一次按键反弹）
  _UpdateLanguageBar(_status, !!langbar_force);

  if (idle) {
    if (_HandleIdleKeyResponse(pContext))
      return;
  }

  _RequestKeyEditSession(pContext, has_commit);
}

bool WeaselTSF::_CompositionStaleMismatch() const {
  return (_IsComposing() != FALSE) != (_status.composing != FALSE);
}

bool WeaselTSF::_HandleIdleKeyResponse(com_ptr<ITfContext> pContext) {
  _HideUI();
  _edit_session_pending = FALSE;
  _deferred.end_requested = FALSE;
  _deferred.end_context = nullptr;
  _deferred.layout_update = FALSE;
  if (_IsComposing()) {
    if (pContext)
      _SyncEndCompositionOnIdle(pContext);
    if (_IsComposing())
      _FinalizeComposition();
  }
  _key_pull.valid = FALSE;
  weasel_crash_diag::Crumb(L"key-ipc", L"idle ok");
  return true;
}

void WeaselTSF::_RequestKeyEditSession(com_ptr<ITfContext> pContext,
                                       bool has_commit) {
  _key_pull.valid = TRUE;
  _ipc_refresh_requested = TRUE;
  _composition_dirty = TRUE;

  const BOOL need_sync =
      has_commit || (!_IsComposing() && _status.composing) ||
      _CompositionStaleMismatch();

  if (!need_sync && _edit_session_pending) {
    weasel_crash_diag::Crumb(L"key-ipc", L"es pending");
    return;
  }

  weasel_crash_diag::Crumb(L"key-ipc", need_sync ? L"es sync" : L"es async");
  _UpdateComposition(pContext, need_sync);
}
