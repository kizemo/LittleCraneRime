#include "stdafx.h"
#include "WeaselTSF.h"
#include <WeaselFileLog.h>
#include <WeaselCrashDiag.h>
#include <WeaselTSFCallbackSafe.h>

static BOOL IsRangeCovered(TfEditCookie ec,
                           ITfRange* pRangeTest,
                           ITfRange* pRangeCover) {
  LONG lResult;

  if (pRangeCover->CompareStart(ec, pRangeTest, TF_ANCHOR_START, &lResult) !=
          S_OK ||
      lResult > 0)
    return FALSE;
  if (pRangeCover->CompareEnd(ec, pRangeTest, TF_ANCHOR_END, &lResult) !=
          S_OK ||
      lResult < 0)
    return FALSE;
  return TRUE;
}

STDAPI WeaselTSF::OnEndEdit(ITfContext* pContext,
                            TfEditCookie ecReadOnly,
                            ITfEditRecord* pEditRecord) {
  return weasel_tsf_safe::RunTSFCallback(L"OnEndEdit", [&]() -> HRESULT {
  try {
    BOOL fSelectionChanged;
    IEnumTfRanges* pEnumTextChanges;
    ITfRange* pRange;

    /* did the selection change? */
    if (pEditRecord && pEditRecord->GetSelectionStatus(&fSelectionChanged) == S_OK &&
        fSelectionChanged) {
      if (_IsComposing()) {
        /* if the caret moves out of composition range, stop the composition */
        TF_SELECTION tfSelection;
        ULONG cFetched;

        if (pContext && pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1,
                                               &tfSelection, &cFetched) == S_OK &&
            cFetched == 1) {
          ITfRange* pRangeComposition;
          if (_pComposition && _pComposition->GetRange(&pRangeComposition) == S_OK) {
            if (!IsRangeCovered(ecReadOnly, tfSelection.range, pRangeComposition))
              _EndComposition(pContext, true);
            pRangeComposition->Release();
          }
        }
      }
    }

    /* text modification? */
    if (pEditRecord && pEditRecord->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, NULL, 0,
                                                              &pEnumTextChanges) == S_OK) {
      if (pEnumTextChanges && pEnumTextChanges->Next(1, &pRange, NULL) == S_OK) {
        if (pRange)
          pRange->Release();
      }
      if (pEnumTextChanges)
        pEnumTextChanges->Release();
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"text-edit",
                     L"uncaught exception in OnEndEdit");
    weasel_crash_diag::Crumb(L"text-edit", L"OnEndEdit ex");
  }
  return S_OK;
  });
}

STDAPI WeaselTSF::OnLayoutChange(ITfContext* pContext,
                                 TfLayoutCode lcode,
                                 ITfContextView* pContextView) {
  return weasel_tsf_safe::RunTSFCallback(L"OnLayoutChange", [&]() -> HRESULT {
  try {
    if (!_IsComposing())
      return S_OK;

    if (pContext != _pTextEditSinkContext)
      return S_OK;

    if (lcode == TF_LC_CHANGE) {
      if (_edit_session_depth > 0) {
        _deferred.layout_update = TRUE;
      } else {
        _UpdateCompositionWindow(pContext);
      }
    }
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"text-edit",
                     L"uncaught exception in OnLayoutChange");
    weasel_crash_diag::Crumb(L"text-edit", L"OnLayoutChange ex");
  }
  return S_OK;
  });
}

BOOL WeaselTSF::_InitTextEditSink(com_ptr<ITfDocumentMgr> pDocMgr) {
  com_ptr<ITfSource> pSource;
  BOOL fRet;

  /* clear out any previous sink first */
  if (_dwTextEditSinkCookie != TF_INVALID_COOKIE) {
    _pTextEditSinkContext->QueryInterface(&pSource);
    if (pSource != nullptr) {
      pSource->UnadviseSink(_dwTextEditSinkCookie);
      pSource->UnadviseSink(_dwTextLayoutSinkCookie);
    }
    _pTextEditSinkContext = nullptr;
    _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  }
  if (pDocMgr == NULL)
    return TRUE;

  if (pDocMgr->GetTop(&_pTextEditSinkContext) != S_OK)
    return FALSE;

  if (_pTextEditSinkContext == NULL)
    return TRUE;

  fRet = FALSE;

  pSource.Release();

  if (_pTextEditSinkContext->QueryInterface(IID_ITfSource, (void**)&pSource) ==
      S_OK) {
    if (pSource->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink*)this,
                            &_dwTextEditSinkCookie) == S_OK)
      fRet = TRUE;
    else
      _dwTextEditSinkCookie = TF_INVALID_COOKIE;
    if (pSource->AdviseSink(IID_ITfTextLayoutSink, (ITfTextLayoutSink*)this,
                            &_dwTextLayoutSinkCookie) == S_OK) {
      fRet = TRUE;
    } else
      _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  }
  if (fRet == FALSE) {
    _pTextEditSinkContext = nullptr;
  }

  return fRet;
}
