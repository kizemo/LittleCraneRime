#pragma once

#include "resource.h"
#include <string>
#include <vector>

class CommonPhraseEditDialog : public CDialogImpl<CommonPhraseEditDialog> {
 public:
  enum { IDD = IDD_COMMON_PHRASE_EDIT };

  CommonPhraseEditDialog();
  ~CommonPhraseEditDialog();

 protected:
  BEGIN_MSG_MAP(CommonPhraseEditDialog)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_SIZE, OnSize)
  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
  COMMAND_ID_HANDLER(IDC_PHRASE_ADD, OnAdd)
  COMMAND_ID_HANDLER(IDC_PHRASE_EDIT, OnEdit)
  COMMAND_ID_HANDLER(IDC_PHRASE_DELETE, OnDelete)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  COMMAND_HANDLER(IDC_PHRASE_LIST, LBN_SELCHANGE, OnListSelChange)
  END_MSG_MAP()

  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnAdd(WORD, WORD, HWND, BOOL&);
  LRESULT OnEdit(WORD, WORD, HWND, BOOL&);
  LRESULT OnDelete(WORD, WORD, HWND, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
  LRESULT OnListSelChange(WORD, WORD, HWND, BOOL&);

  void BuildUi();
  void LayoutUi(int cx, int cy);
  void ReloadList();
  bool SavePhrases();
  int GetSelectedIndex() const;

  CListBox list_;
  CEdit input_edit_;
  CButton btn_add_;
  CButton btn_edit_;
  CButton btn_delete_;
  CButton btn_ok_;
  CButton btn_cancel_;
  std::vector<std::wstring> phrases_;
  HFONT font_normal_ = NULL;
  HFONT font_title_ = NULL;
  bool ui_built_ = false;
};
