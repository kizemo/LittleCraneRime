#pragma once

#include "resource.h"
#include <string>

class CustomPhraseDialog : public CDialogImpl<CustomPhraseDialog> {
 public:
  enum { IDD = IDD_CUSTOM_PHRASE };

  CustomPhraseDialog();
  ~CustomPhraseDialog();

  void SetInitial(const std::wstring& code, const std::wstring& phrase);
  std::wstring GetCode() const { return code_; }
  std::wstring GetPhrase() const { return phrase_; }

 protected:
  BEGIN_MSG_MAP(CustomPhraseDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);

  bool ValidateInput(std::wstring& code, std::wstring& phrase);
  std::wstring GetPhraseFileName();

  CEdit code_edit_;
  CEdit phrase_edit_;
  std::wstring initial_code_;
  std::wstring initial_phrase_;
  std::wstring code_;
  std::wstring phrase_;
};
