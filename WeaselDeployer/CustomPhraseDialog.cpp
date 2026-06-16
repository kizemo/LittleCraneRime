#include "stdafx.h"
#include "CustomPhraseDialog.h"
#include <WeaselUtility.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

CustomPhraseDialog::CustomPhraseDialog() {}

CustomPhraseDialog::~CustomPhraseDialog() {}

void CustomPhraseDialog::SetInitial(const std::wstring& code,
                                    const std::wstring& phrase) {
  initial_code_ = code;
  initial_phrase_ = phrase;
}

std::wstring CustomPhraseDialog::GetPhraseFileName() {
  fs::path user_dir = WeaselUserDataPath();
  fs::path double_file = user_dir / L"custom_phrase_double.txt";
  fs::path full_file = user_dir / L"custom_phrase.txt";

  if (fs::exists(double_file) && fs::file_size(double_file) > 0)
    return double_file.wstring();

  fs::path default_custom = user_dir / L"default.custom.yaml";
  if (fs::exists(default_custom)) {
    std::ifstream in(default_custom);
    std::string line;
    while (std::getline(in, line)) {
      if (line.find("double_pinyin") != std::string::npos)
        return double_file.wstring();
    }
  }

  if (fs::exists(full_file))
    return full_file.wstring();
  return double_file.wstring();
}

LRESULT CustomPhraseDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  code_edit_.Attach(GetDlgItem(IDC_PHRASE_CODE));
  phrase_edit_.Attach(GetDlgItem(IDC_PHRASE_TEXT));
  if (!initial_code_.empty())
    code_edit_.SetWindowTextW(initial_code_.c_str());
  if (!initial_phrase_.empty())
    phrase_edit_.SetWindowTextW(initial_phrase_.c_str());
  CenterWindow();
  code_edit_.SetFocus();
  return FALSE;
}

LRESULT CustomPhraseDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

bool CustomPhraseDialog::ValidateInput(std::wstring& code,
                                       std::wstring& phrase) {
  wchar_t code_buf[256] = {0};
  wchar_t phrase_buf[4096] = {0};
  code_edit_.GetWindowTextW(code_buf, 256);
  phrase_edit_.GetWindowTextW(phrase_buf, 4096);
  code = code_buf;
  phrase = phrase_buf;
  if (code.empty() || phrase.empty())
    return false;
  for (wchar_t c : code) {
    if (!iswalnum(c) && c != L'_' && c != L'-' && c != L'\'')
      return false;
  }
  return true;
}

LRESULT CustomPhraseDialog::OnOK(WORD, WORD, HWND, BOOL&) {
  if (!ValidateInput(code_, phrase_)) {
    ::MessageBoxW(m_hWnd, L"请填写有效的编码和短语内容。\n编码仅支持字母、数字、下划线和连字符。",
                  L"【小鹅】自定义短语", MB_OK | MB_ICONWARNING);
    return 0;
  }
  EndDialog(IDOK);
  return 0;
}

LRESULT CustomPhraseDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}
