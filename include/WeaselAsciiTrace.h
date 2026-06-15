#pragma once

#include <WeaselFileLog.h>

#include <cstdio>

inline void WeaselAsciiTrace(const wchar_t* source, const wchar_t* detail) {
  WeaselAppendLogW(L"weasel.ascii.log", source, detail);
}

inline void WeaselAsciiTraceMode(bool from_ascii,
                                 bool to_ascii,
                                 const wchar_t* source,
                                 const wchar_t* extra = L"") {
  wchar_t buf[384];
  swprintf_s(buf, L"ascii %d -> %d | %s", from_ascii ? 1 : 0, to_ascii ? 1 : 0,
             extra);
  WeaselAsciiTrace(source, buf);
}

inline void WeaselAsciiTraceShiftCombo(DWORD vk, const wchar_t* via) {
  wchar_t buf[128];
  swprintf_s(buf, L"shift combo vk=0x%X via=%s", vk, via);
  WeaselAsciiTrace(L"shift-guard", buf);
}

inline void WeaselAsciiTraceShiftRelease(bool eaten, int physical_keys, bool tsf_combo) {
  wchar_t buf[128];
  swprintf_s(buf, L"shift release eaten=%d physical_keys=%d tsf_combo=%d",
             eaten ? 1 : 0, physical_keys, tsf_combo ? 1 : 0);
  WeaselAsciiTrace(L"shift-guard", buf);
}
