// process_closer.cpp
//
// Implementation of the auto-close pipeline used by the Weasel installer to
// release locks on System32\weasel*.dll held by long-running hosts
// (Directory Opus, WeChat, TRAE SOLO, Cursor, ...).
//
// Build requires linking against rstrtmgr.lib (Restart Manager). The header
// <restartmanager.h> ships with the Windows SDK since Vista.

#include "stdafx.h"
#include "process_closer.h"
#include <restartmanager.h>
#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <WeaselFileLog.h>

#pragma comment(lib, "rstrtmgr.lib")
#pragma comment(lib, "shlwapi.lib")

namespace weasel_setup {

namespace {

// Lower-case basename only.
std::wstring BasenameLower(const std::wstring& p) {
    std::wstring out = p;
    const size_t sep = out.find_last_of(L"\\/");
    if (sep != std::wstring::npos)
        out.erase(0, sep + 1);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](wchar_t c) { return (wchar_t)towlower(c); });
    return out;
}

// Process names that must NEVER be terminated, even if they hold the dll.
// Killing any of these will hard-crash Windows or destroy the user's session.
const wchar_t* const kCriticalProcesses[] = {
    L"csrss.exe", L"winlogon.exe", L"lsass.exe", L"services.exe",
    L"smss.exe",  L"system",       L"svchost.exe",
    L"dwm.exe",   L"explorer.exe", L"taskmgr.exe",
    L"weaselsetup.exe",  // self
};

bool IsCriticalProcess(const std::wstring& app_name) {
    const std::wstring low = BasenameLower(app_name);
    for (const auto* name : kCriticalProcesses) {
        if (low == name)
            return true;
    }
    return false;
}

bool PidIsAlive(DWORD pid) {
    if (pid == 0 || pid == GetCurrentProcessId())
        return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return false;
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

// Walk top-level windows belonging to `pid` and post WM_CLOSE.
struct PidEnumCtx {
    DWORD pid;
    std::vector<HWND> windows;
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<PidEnumCtx*>(lParam);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid == ctx->pid && IsWindowVisible(hwnd)) {
        ctx->windows.push_back(hwnd);
    }
    return TRUE;
}

void CloseWindowsOfPid(DWORD pid) {
    PidEnumCtx ctx{pid, {}};
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));
    for (HWND h : ctx.windows) {
        PostMessage(h, WM_CLOSE, 0, 0);
    }
}

// Try to terminate a process by PID, except for critical ones.
bool TryTerminatePid(DWORD pid) {
    if (!PidIsAlive(pid))
        return true;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) {
        // Probably a protected process; nothing we can do.
        return false;
    }
    const bool ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok;
}

}  // namespace

bool FindProcessesHoldingFile(const std::wstring& target_path,
                              std::vector<HoldingProcess>* out_processes) {
    if (!out_processes)
        return false;
    out_processes->clear();

    DWORD session = 0;
    wchar_t key[CCH_RM_SESSION_KEY + 1] = {};
    HRESULT hr = RmStartSession(&session, 0, key);
    if (FAILED(hr)) {
        WeaselAppendLogW(L"install.log", L"process-closer",
                         std::wstring(L"RmStartSession failed hr=0x") +
                             std::to_wstring((unsigned long)hr));
        return false;
    }

    // Register the path as a file resource.
    const wchar_t* files[1] = {target_path.c_str()};
    hr = RmRegisterResources(session, 1, files,
                             0, nullptr, 0, nullptr);
    if (FAILED(hr)) {
        WeaselAppendLogW(L"install.log", L"process-closer",
                         std::wstring(L"RmRegisterResources failed hr=0x") +
                             std::to_wstring((unsigned long)hr));
        RmEndSession(session);
        return false;
    }

    UINT needed = 0;
    UINT count = 0;
    DWORD reboot_reasons = 0;
    // Probe for required buffer size.
    DWORD rc = RmGetList(session, &needed, &count, nullptr, &reboot_reasons);
    std::vector<RM_PROCESS_INFO> infos;
    if (rc == ERROR_MORE_DATA) {
        infos.resize(needed / sizeof(RM_PROCESS_INFO) + 1);
        rc = RmGetList(session, &needed, &count, infos.data(), &reboot_reasons);
    }
    if (rc != 0) {
        WeaselAppendLogW(L"install.log", L"process-closer",
                         std::wstring(L"RmGetList failed rc=") +
                             std::to_wstring((unsigned long)rc));
        RmEndSession(session);
        return false;
    }

    out_processes->reserve(count);
    for (UINT i = 0; i < count; ++i) {
        HoldingProcess hp;
        hp.pid = infos[i].Process.dwProcessId;
        hp.app_name = infos[i].strAppName[0] ? infos[i].strAppName : L"";
        hp.full_path = infos[i].strServiceShortName[0]
                          ? infos[i].strServiceShortName
                          : L"";
        hp.critical = IsCriticalProcess(hp.app_name);
        out_processes->push_back(hp);
    }
    RmEndSession(session);
    return true;
}

CloseOutcome CloseProcessesHoldingFile(
    const std::wstring& target_path,
    DWORD grace_ms,
    std::vector<HoldingProcess>* out_attempted) {

    if (out_attempted)
        out_attempted->clear();

    std::vector<HoldingProcess> holders;
    if (!FindProcessesHoldingFile(target_path, &holders)) {
        return CloseOutcome::kInternalError;
    }
    if (holders.empty()) {
        WeaselAppendLogW(L"install.log", L"process-closer",
                         L"no holders for " + target_path);
        return CloseOutcome::kAlreadyFree;
    }

    // Filter out critical processes but report them in the log so the user
    // can see why we couldn't proceed with them.
    std::vector<HoldingProcess> closeable;
    for (const auto& h : holders) {
        if (h.critical) {
            wchar_t buf[256];
            swprintf_s(buf, L"skip critical holder pid=%lu app=%s",
                       h.pid, h.app_name.c_str());
            WeaselAppendLogW(L"install.log", L"process-closer", buf);
            continue;
        }
        closeable.push_back(h);
    }

    if (closeable.empty()) {
        // All holders are critical - we cannot close any of them. Caller
        // will see kSomeStillLocked so it knows to try MOVEFILE_DELAY_UNTIL_REBOOT.
        if (out_attempted) *out_attempted = holders;
        return CloseOutcome::kSomeStillLocked;
    }

    WeaselAppendLogW(L"install.log", L"process-closer",
                     std::wstring(L"attempting to close ") +
                         std::to_wstring((unsigned long)closeable.size()) +
                         L" process(es) for " + target_path);

    // Stage 1: broadcast WM_QUERYENDSESSION so apps can save state.
    // This is a no-op for processes that don't handle it, but those that
    // do (Outlook, editors, ...) will prompt the user or save silently.
    for (const auto& h : closeable) {
        // Per-process WM_QUERYENDSESSION via its windows.
        PidEnumCtx ctx{h.pid, {}};
        EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));
        for (HWND w : ctx.windows) {
            PostMessage(w, WM_QUERYENDSESSION, 0,
                        ENDSESSION_LOGOFF | ENDSESSION_CLOSEAPP);
        }
    }

    // Stage 2: WM_CLOSE to top-level windows.
    for (const auto& h : closeable) {
        CloseWindowsOfPid(h.pid);
    }

    // Stage 3: graceful wait.
    const DWORD slice = 200;
    DWORD waited = 0;
    while (waited < grace_ms) {
        bool any_alive = false;
        for (const auto& h : closeable) {
            if (PidIsAlive(h.pid)) {
                any_alive = true;
                break;
            }
        }
        if (!any_alive) break;
        Sleep(slice);
        waited += slice;
    }

    // Stage 4: forced termination of survivors.
    int killed = 0, survivors = 0;
    for (const auto& h : closeable) {
        if (PidIsAlive(h.pid)) {
            if (TryTerminatePid(h.pid)) {
                ++killed;
            } else {
                ++survivors;
                wchar_t buf[256];
                swprintf_s(buf, L"failed to terminate pid=%lu app=%s",
                           h.pid, h.app_name.c_str());
                WeaselAppendLogW(L"install.log", L"process-closer", buf);
            }
        }
    }

    // Final brief wait for kernel cleanup.
    Sleep(500);

    wchar_t buf[256];
    swprintf_s(buf, L"close pipeline done killed=%d survivors=%d",
               killed, survivors);
    WeaselAppendLogW(L"install.log", L"process-closer", buf);

    if (out_attempted)
        *out_attempted = closeable;

    return survivors == 0 ? CloseOutcome::kClosedAll
                          : CloseOutcome::kSomeStillLocked;
}

}  // namespace weasel_setup
