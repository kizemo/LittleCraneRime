// process_closer.h
//
// Detects and (politely then forcibly) closes processes that hold a lock on
// files we are about to replace during installation. Used so the installer
// can succeed even when a long-running host (e.g. Directory Opus, WeChat,
// TRAE SOLO) is keeping the existing weasel.dll open in System32.
//
// Strategy (in order):
//   1. Restart Manager session -> enumerate PIDs holding the file.
//   2. WM_QUERYENDSESSION broadcast to give apps a chance to save state.
//   3. Per-PID WM_CLOSE to top-level windows of the offending process.
//   4. Wait up to a configurable grace period.
//   5. TerminateProcess() fallback for anything still alive.
// White-listed critical system processes are NEVER terminated.

#pragma once

#include <string>
#include <vector>

namespace weasel_setup {

struct HoldingProcess {
    DWORD pid = 0;
    std::wstring app_name;     // e.g. "Directory Opus.exe"
    std::wstring full_path;    // executable on disk, if known
    bool critical = false;     // true if on the protected whitelist
};

enum class CloseOutcome {
    kAlreadyFree,        // nobody held the file
    kClosedAll,          // at least one holder was closed successfully
    kSomeStillLocked,    // some processes refused to die
    kInternalError,      // Restart Manager itself failed
};

// Find processes that have a handle/load on `target_path`.
// Returns true on success (the vector may still be empty).
bool FindProcessesHoldingFile(const std::wstring& target_path,
                              std::vector<HoldingProcess>* out_processes);

// Run the full close pipeline for processes holding `target_path`.
// `grace_ms` is the time we wait between the polite messages and the kill.
CloseOutcome CloseProcessesHoldingFile(const std::wstring& target_path,
                                       DWORD grace_ms,
                                       std::vector<HoldingProcess>* out_attempted);

}  // namespace weasel_setup
