#include "stdafx.h"
#include "WeaselTSF.h"
#include <WeaselTSFCallbackSafe.h>

STDAPI WeaselTSF::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) {
  return weasel_tsf_safe::RunTSFCallback(L"OnInitDocumentMgr",
                                         [&]() -> HRESULT {
    return S_OK;
  });
}

STDAPI WeaselTSF::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) {
  return weasel_tsf_safe::RunTSFCallback(L"OnUninitDocumentMgr",
                                         [&]() -> HRESULT {
    return S_OK;
  });
}

STDAPI WeaselTSF::OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                             ITfDocumentMgr* pDocMgrPrevFocus) {
  return weasel_tsf_safe::RunTSFCallback(L"OnSetFocus", [&]() -> HRESULT {
    _InitTextEditSink(pDocMgrFocus);

    com_ptr<ITfDocumentMgr> pCandidateListDocumentMgr;
    com_ptr<ITfContext> pTfContext = _GetUIContextDocument();
    if ((nullptr != pTfContext) &&
        SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr))) {
      if (pCandidateListDocumentMgr != pDocMgrFocus) {
        _HideUI();
      } else {
        _ShowUI();
      }
    }

    return S_OK;
  });
}

STDAPI WeaselTSF::OnPushContext(ITfContext* pContext) {
  return weasel_tsf_safe::RunTSFCallback(L"OnPushContext", [&]() -> HRESULT {
    return S_OK;
  });
}

STDAPI WeaselTSF::OnPopContext(ITfContext* pContext) {
  return weasel_tsf_safe::RunTSFCallback(L"OnPopContext", [&]() -> HRESULT {
    return S_OK;
  });
}

BOOL WeaselTSF::_InitThreadMgrEventSink() {
  ITfSource* pSource;
  if (_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource) != S_OK)
    return FALSE;
  if (pSource->AdviseSink(IID_ITfThreadMgrEventSink,
                          (ITfThreadMgrEventSink*)this,
                          &_dwThreadMgrEventSinkCookie) != S_OK) {
    _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
    pSource->Release();
    return FALSE;
  }
  pSource->Release();
  return TRUE;
}

void WeaselTSF::_UninitThreadMgrEventSink() {
  ITfSource* pSource;
  if (_dwThreadMgrEventSinkCookie == TF_INVALID_COOKIE)
    return;
  if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
    pSource->UnadviseSink(_dwThreadMgrEventSinkCookie);
    pSource->Release();
  }
  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
}
