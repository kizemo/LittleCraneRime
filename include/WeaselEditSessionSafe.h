#pragma once
#include <WeaselFileLog.h>

namespace weasel {

template <typename Fn>
HRESULT RunEditSessionSafe(const wchar_t* scope, Fn&& fn) {
  try {
    return fn();
  } catch (const std::exception&) {
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     std::wstring(L"C++ exception in ") + scope);
    return E_FAIL;
  } catch (...) {
    WeaselAppendLogW(L"weasel.tsf.log", L"edit-session",
                     std::wstring(L"unknown exception in ") + scope);
    return E_FAIL;
  }
}

}  // namespace
