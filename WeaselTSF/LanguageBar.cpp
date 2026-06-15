#include "stdafx.h"
#include <resource.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "LanguageBar.h"
#include "CandidateList.h"
#include "ResponseParser.h"
#include <WeaselUtility.h>
#include <WeaselFileLog.h>
#include <ShiftComboGuard.h>

static const DWORD LANGBARITEMSINK_COOKIE = 0x42424242;

static void HMENU2ITfMenu(HMENU hMenu, ITfMenu* pTfMenu) {
  /* NOTE: Only limited functions are supported */
  int N = GetMenuItemCount(hMenu);
  for (int i = 0; i < N; i++) {
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
    mii.dwTypeData = NULL;
    if (GetMenuItemInfo(hMenu, i, TRUE, &mii)) {
      UINT id = mii.wID;
      if (mii.fType == MFT_SEPARATOR)
        pTfMenu->AddMenuItem(id, TF_LBMENUF_SEPARATOR, NULL, NULL, NULL, 0,
                             NULL);
      else if (mii.fType == MFT_STRING) {
        mii.dwTypeData = (LPWSTR)malloc(sizeof(WCHAR) * (mii.cch + 1));
        mii.cch++;
        if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
          pTfMenu->AddMenuItem(id, 0, NULL, NULL, mii.dwTypeData, mii.cch,
                               NULL);
        free(mii.dwTypeData);
      }
    }
  }
}

static LPCWSTR GetWeaselRegName() {
  LPCWSTR WEASEL_REG_NAME_;
  if (is_wow64())
    WEASEL_REG_NAME_ = L"Software\\WOW6432Node\\Rime\\Weasel";
  else
    WEASEL_REG_NAME_ = L"Software\\Rime\\Weasel";

  return WEASEL_REG_NAME_;
}

static bool open(const std::wstring& path) {
  return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                  SW_SHOWNORMAL) > 32;
}

static bool explore(const std::wstring& path) {
  std::wstring quoted_path = L"\"" + path + L"\"";
  return (uintptr_t)ShellExecuteW(NULL, L"explore", quoted_path.c_str(), NULL,
                                  NULL, SW_SHOWNORMAL) > 32;
}

CLangBarItemButton::CLangBarItemButton(com_ptr<WeaselTSF> pTextService,
                                       REFGUID guid,
                                       weasel::UIStyle& style)
    : _status(0),
      _style(style),
      _current_schema_zhung_icon(),
      _current_schema_ascii_icon() {
  DllAddRef();

  _pLangBarItemSink = NULL;
  _cRef = 1;
  _pTextService = pTextService;
  _guid = guid;
  ascii_mode = false;
}

CLangBarItemButton::~CLangBarItemButton() {
  DllRelease();
}

STDAPI CLangBarItemButton::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
      IsEqualIID(riid, IID_ITfLangBarItemButton))
    *ppvObject = (ITfLangBarItemButton*)this;
  else if (IsEqualIID(riid, IID_ITfSource))
    *ppvObject = (ITfSource*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) CLangBarItemButton::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) CLangBarItemButton::Release() {
  LONG cr = --_cRef;
  assert(_cRef >= 0);
  if (_cRef == 0)
    delete this;
  return cr;
}

STDAPI CLangBarItemButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
  pInfo->clsidService = c_clsidTextService;
  pInfo->guidItem = _guid;
  // Do not use SHOWNINTRAY: WeaselServer already registers one tray icon.
  pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU;
  pInfo->ulSort = 1;
  lstrcpyW(pInfo->szDescription, L"WeaselTSF Button");
  return S_OK;
}

STDAPI CLangBarItemButton::GetStatus(DWORD* pdwStatus) {
  *pdwStatus = _status;
  if (_style.display_tray_icon)
    *pdwStatus |= TF_LBI_STATUS_HIDDEN;
  return S_OK;
}

STDAPI CLangBarItemButton::Show(BOOL fShow) {
  SetLangbarStatus(TF_LBI_STATUS_HIDDEN, fShow ? FALSE : TRUE);
  return S_OK;
}

static LANGID GetActiveProfileLangId() {
  CComPtr<ITfInputProcessorProfileMgr> pInputProcessorProfileMgr;
  HRESULT hr = pInputProcessorProfileMgr.CoCreateInstance(
      CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_ALL);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

  TF_INPUTPROCESSORPROFILE profile;
  hr = pInputProcessorProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,
                                                   &profile);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  return profile.langid;
}

STDAPI CLangBarItemButton::GetTooltipString(BSTR* pbstrToolTip) {
  LANGID langid = get_language_id();
  if (langid == TEXTSERVICE_LANGID_HANS) {
    *pbstrToolTip = SysAllocString(L"左键切换模式，右键打开菜单");
  } else if (langid == TEXTSERVICE_LANGID_HANT) {
    *pbstrToolTip = SysAllocString(L"左鍵切換模式，右鍵打開菜單");
  } else {
    *pbstrToolTip = SysAllocString(
        L"Left-click to switch modes\n\nRight-click for more options");
  }

  return (*pbstrToolTip == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDAPI CLangBarItemButton::OnClick(TfLBIClick click,
                                   POINT pt,
                                   const RECT* prcArea) {
  if (click == TF_LBI_CLK_LEFT) {
    _pTextService->ShowQuickBar(pt);
  } else if (click == TF_LBI_CLK_RIGHT) {
    HWND hwnd = _pTextService->_GetFocusedContextWindow();
    if (!hwnd)
      hwnd = GetForegroundWindow();
    if (!hwnd)
      hwnd = GetDesktopWindow();
    {
      LANGID langid = get_language_id();
      HMENU menu;
      if (langid == TEXTSERVICE_LANGID_HANS) {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP_HANS));
      } else if (langid == TEXTSERVICE_LANGID_HANT) {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP_HANT));
      } else {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
      }
      HMENU popupMenu = GetSubMenu(menu, 0);
      UINT wID = TrackPopupMenuEx(
          popupMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_HORPOSANIMATION, pt.x,
          pt.y, hwnd, NULL);
      DestroyMenu(menu);
      _pTextService->_HandleLangBarMenuSelect(wID);
    }
  }
  return S_OK;
}

STDAPI CLangBarItemButton::InitMenu(ITfMenu* pMenu) {
  HMENU menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
  HMENU popupMenu = GetSubMenu(menu, 0);
  HMENU2ITfMenu(popupMenu, pMenu);
  DestroyMenu(menu);
  return S_OK;
}

STDAPI CLangBarItemButton::OnMenuSelect(UINT wID) {
  _pTextService->_HandleLangBarMenuSelect(wID);
  return S_OK;
}

STDAPI CLangBarItemButton::GetIcon(HICON* phIcon) {
  if (ascii_mode) {
    if (_style.current_ascii_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_EN), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_ascii_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  } else {
    if (_style.current_zhung_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_ZH), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_zhung_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  }
  return (*phIcon == NULL) ? E_FAIL : S_OK;
}

STDAPI CLangBarItemButton::GetText(BSTR* pbstrText) {
  *pbstrText = SysAllocString(L"WeaselTSF Button");
  return (*pbstrText == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDAPI CLangBarItemButton::AdviseSink(REFIID riid,
                                      IUnknown* punk,
                                      DWORD* pdwCookie) {
  if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
    return CONNECT_E_CANNOTCONNECT;
  if (_pLangBarItemSink != NULL)
    return CONNECT_E_ADVISELIMIT;

  if (punk->QueryInterface(IID_ITfLangBarItemSink,
                           (LPVOID*)&_pLangBarItemSink) != S_OK) {
    _pLangBarItemSink = NULL;
    return E_NOINTERFACE;
  }
  *pdwCookie = LANGBARITEMSINK_COOKIE;
  return S_OK;
}

STDAPI CLangBarItemButton::UnadviseSink(DWORD dwCookie) {
  if (dwCookie != LANGBARITEMSINK_COOKIE || _pLangBarItemSink == NULL)
    return CONNECT_E_NOCONNECTION;
  _pLangBarItemSink = NULL;
  return S_OK;
}

void CLangBarItemButton::UpdateWeaselStatus(weasel::Status stat) {
  bool changed = false;
  if (stat.ascii_mode != ascii_mode) {
    ascii_mode = stat.ascii_mode;
    changed = true;
  }
  if (_current_schema_zhung_icon != _style.current_zhung_icon) {
    _current_schema_zhung_icon = _style.current_zhung_icon;
    changed = true;
  }
  if (_current_schema_ascii_icon != _style.current_ascii_icon) {
    _current_schema_ascii_icon = _style.current_ascii_icon;
    changed = true;
  }
  if (changed && _pLangBarItemSink) {
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
  }
}

void CLangBarItemButton::SetLangbarStatus(DWORD dwStatus, BOOL fSet) {
  BOOL isChange = FALSE;

  if (fSet) {
    if (!(_status & dwStatus)) {
      _status |= dwStatus;
      isChange = TRUE;
    }
  } else {
    if (_status & dwStatus) {
      _status &= ~dwStatus;
      isChange = TRUE;
    }
  }

  if (isChange && _pLangBarItemSink) {
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
  }

  return;
}

std::wstring WeaselTSF::_GetRootDir() {
  std::wstring dir{};
  RegGetStringValue(HKEY_LOCAL_MACHINE, GetWeaselRegName(), L"WeaselRoot", dir);
  return dir;
}

bool WeaselTSF::_HasCandidateMenu() {
  if (!_cand || _status.ascii_mode)
    return false;
  UINT count = 0;
  if (_cand->GetCount(&count) != S_OK)
    return false;
  return count > 0;
}

bool WeaselTSF::_CanDeleteCandidate() {
  if (!_cand || !_status.composing)
    return false;
  UINT count = 0;
  if (_cand->GetCount(&count) != S_OK)
    return false;
  return count > 0;
}

void WeaselTSF::_ToggleAsciiMode(BOOL lock_rime) {
  if (!_EnsureIpcSession()) {
    WeaselAppendLogW(L"weasel.tsf.log", L"toggle",
                     L"_ToggleAsciiMode aborted: no ipc session");
    return;
  }
  ShiftComboGuard::OnShiftKeyUpAlone();

  const bool ascii_before = !!_status.ascii_mode;
  const bool target_ascii = !ascii_before;

  m_client.SetAsciiMode(target_ascii);

  if (!_RefreshStatusFromResponse() || !!_status.ascii_mode != target_ascii) {
    _status.ascii_mode = target_ascii;
    _status.full_shape = target_ascii;
  }

  _ascii_toggle_value_ = !!_status.ascii_mode;
  _ascii_toggle_tick_ = GetTickCount();
  // 0.18.97: 仅当 shift 单独释放触发（lock_rime=true）时锁定 RIME 状态
  _ascii_toggle_lock_rime_ = lock_rime;
  wchar_t buf[128];
  swprintf_s(buf, L"ToggleAsciiMode ok session=%lu ascii=%d lock_rime=%d",
             static_cast<unsigned long>(m_client.SessionId()),
             _status.ascii_mode ? 1 : 0, lock_rime ? 1 : 0);
  WeaselAppendLogW(L"weasel.tsf.log", L"toggle", buf);
  _UpdateLanguageBar(_status, true);
}

bool WeaselTSF::_RefreshStatusFromResponse() {
  std::wstring commit;
  weasel::Status status = _status;
  weasel::Config config;
  auto context = std::make_shared<weasel::Context>();
  weasel::ResponseParser parser(&commit, context.get(), &status, &config,
                                &_cand->style());
  if (!m_client.GetResponseData(parser))
    return false;
  _status = status;
  return true;
}


void WeaselTSF::ShowQuickBar(POINT pt) {
  WeaselAppendLogW(L"weasel.tsf.log", L"quickbar",
                   L"show quickbar from langbar click");
  if (!_EnsureServerConnected()) {
    WeaselAppendLogW(L"weasel.tsf.log", L"quickbar",
                     L"failed: server not connected");
    return;
  }
  GetCursorPos(&pt);
  m_client.TrayCommand(ID_WEASELTRAY_QUICK_BAR, MAKELPARAM(pt.x, pt.y));
}

void WeaselTSF::_HandleLangBarMenuSelect(UINT wID) {
  std::wstring dir{};
  switch (wID) {
    case ID_WEASELTRAY_RERUN_SERVICE:
    case ID_WEASELTRAY_INSTALLDIR:
      if (RegGetStringValue(HKEY_LOCAL_MACHINE, GetWeaselRegName(),
                            L"WeaselRoot", dir) == ERROR_SUCCESS) {
        if (wID == ID_WEASELTRAY_RERUN_SERVICE) {
          std::thread th([dir]() {
            ShellExecuteW(NULL, L"open", (dir + L"\\start_service.bat").c_str(),
                          NULL, dir.c_str(), SW_HIDE);
          });
          th.detach();
        } else
          explore(dir);
      }
      break;
    case ID_WEASELTRAY_USERCONFIG:
      if (FAILED(RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel",
                                   L"RimeUserDir", dir)) ||
          dir.empty()) {
        WCHAR _path[MAX_PATH] = {0};
        ExpandEnvironmentStringsW(L"%AppData%\\Rime", _path, _countof(_path));
        dir = std::wstring(_path);
      }
      if (!dir.empty() && fs::exists(dir))
        explore(dir);
      else
        MessageBoxW(NULL, (L"Not found: " + dir).c_str(), L"RimeUserDir",
                    MB_ICONERROR | MB_OK);
      break;
    case ID_WEASELTRAY_LOGDIR:
      explore(WeaselLogPath().wstring());
      break;
    case ID_WEASELTRAY_WIKI:
      open(L"https://rime.im/docs/");
      break;
    case ID_WEASELTRAY_FORUM:
      open(L"https://rime.im/discuss/");
      break;
    default: {
      wchar_t logbuf[64];
      swprintf_s(logbuf, L"langbar tray cmd id=%u", wID);
      WeaselAppendLogW(L"weasel.tsf.log", L"langbar", logbuf);
      m_client.TrayCommand(wID);
      break;
    }
  }
}

HWND WeaselTSF::_GetFocusedContextWindow() {
  HWND hwnd = NULL;
  ITfDocumentMgr* pDocMgr;
  if (_pThreadMgr->GetFocus(&pDocMgr) == S_OK && pDocMgr != NULL) {
    ITfContext* pContext;
    if (pDocMgr->GetTop(&pContext) == S_OK && pContext != NULL) {
      ITfContextView* pContextView;
      if (pContext->GetActiveView(&pContextView) == S_OK &&
          pContextView != NULL) {
        pContextView->GetWnd(&hwnd);
        pContextView->Release();
      }
      pContext->Release();
    }
    pDocMgr->Release();
  }

  if (hwnd == NULL) {
    HWND hwndForeground = GetForegroundWindow();
    if (GetWindowThreadProcessId(hwndForeground, NULL) == GetCurrentThreadId())
      hwnd = hwndForeground;
  }

  return hwnd;
}

BOOL WeaselTSF::_InitLanguageBar() {
  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;
  BOOL fRet = FALSE;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) != S_OK)
    return FALSE;

  if ((_pLangBarButton = new CLangBarItemButton(this, GUID_LBI_INPUTMODE,
                                                _cand->style())) == NULL)
    return FALSE;

  if (pLangBarItemMgr->AddItem(_pLangBarButton) != S_OK) {
    _pLangBarButton = NULL;
    return FALSE;
  }

  // Hide langbar until style sync; default distribution uses server tray icon.
  _pLangBarButton->Show(FALSE);
  _SyncLanguageBarTrayVisibility();
  fRet = TRUE;

  return fRet;
}

void WeaselTSF::_UninitLanguageBar() {
  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;

  if (_pLangBarButton == NULL)
    return;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) == S_OK) {
    pLangBarItemMgr->RemoveItem(_pLangBarButton);
  }

  _pLangBarButton = NULL;
}

void WeaselTSF::_UpdateLanguageBar(weasel::Status stat, bool force_compartment_sync) {
  if (!_pLangBarButton)
    return;
  static bool last_ascii = false;
  static bool last_full = false;
  static DWORD last_sync_tick = 0;
  DWORD now = GetTickCount();
  bool mode_changed =
      stat.ascii_mode != last_ascii || stat.full_shape != last_full;
  bool debounce_ok = now - last_sync_tick >= 100;
  const bool sync_compartment =
      force_compartment_sync ||
      (!ShiftComboGuard::ShouldIgnoreAsciiSync() && mode_changed && debounce_ok);

  if (sync_compartment) {
    if (force_compartment_sync)
      ShiftComboGuard::g_block_ascii_sync_until = 0;
    DWORD flags = 0;
    _GetCompartmentDWORD(flags, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
    DWORD new_flags = flags;
    if (stat.ascii_mode)
      new_flags &= (~TF_CONVERSIONMODE_NATIVE);
    else
      new_flags |= TF_CONVERSIONMODE_NATIVE;
    if (stat.full_shape)
      new_flags |= TF_CONVERSIONMODE_FULLSHAPE;
    else
      new_flags &= (~TF_CONVERSIONMODE_FULLSHAPE);
    if (new_flags != flags) {
      _isSyncingCompartment = TRUE;
      _SetCompartmentDWORD(new_flags,
                           GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
      _isSyncingCompartment = FALSE;
      last_ascii = stat.ascii_mode;
      last_full = stat.full_shape;
      last_sync_tick = now;
    } else if (force_compartment_sync) {
      last_ascii = stat.ascii_mode;
      last_full = stat.full_shape;
      last_sync_tick = now;
    }
  }

  _pLangBarButton->UpdateWeaselStatus(stat);
  _SyncLanguageBarTrayVisibility();
}

void WeaselTSF::_ShowLanguageBar(BOOL show) {
  if (!_pLangBarButton)
    return;
  _pLangBarButton->Show(show);
}

void WeaselTSF::_SyncLanguageBarTrayVisibility() {
  if (!_pLangBarButton)
    return;
  // When WeaselServer shows its tray icon, hide the TSF langbar button to avoid
  // duplicate "中/EN" icons on the taskbar.
  const BOOL show_langbar = _cand->style().display_tray_icon ? FALSE : TRUE;
  static int s_last_visible = -1;
  static int s_last_tray = -1;
  const int visible = show_langbar ? 1 : 0;
  const int tray = _cand->style().display_tray_icon ? 1 : 0;
  _pLangBarButton->Show(show_langbar);
  if (_cand->style().display_tray_icon)
    _pLangBarButton->SetLangbarStatus(TF_LBI_STATUS_HIDDEN, TRUE);
  if (visible != s_last_visible || tray != s_last_tray) {
    wchar_t buf[64];
    swprintf_s(buf, L"langbar visible=%d display_tray_icon=%d", visible, tray);
    WeaselAppendLogW(L"weasel.tsf.log", L"tray", buf);
    s_last_visible = visible;
    s_last_tray = tray;
  }
}

void WeaselTSF::_EnableLanguageBar(BOOL enable) {
  if (!_pLangBarButton)
    return;
  (void)enable;
  _pLangBarButton->SetLangbarStatus(TF_LBI_STATUS_DISABLED, FALSE);
}
