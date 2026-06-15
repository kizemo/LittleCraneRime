#pragma once

#include <windows.h>

// TSF-layer guard only (no global keyboard hooks).
// Rime ascii_composer already implements chord_key_pressed_ per librime design.

namespace ShiftComboGuard {

inline bool g_shift_armed = false;
inline bool g_shift_combo = false;
inline bool g_non_shift_key_seen = false;
inline bool g_letter_key_seen = false;
inline DWORD g_block_ascii_sync_until = 0;

constexpr DWORD kAsciiSyncBlockMs = 1500;

// Forward declaration: defined after ShouldBlockShiftToggle
inline bool IsAsciiSyncRecentlyBlocked();

inline bool IsLetterOrDigitVKey(WPARAM vkey) {
  if (vkey >= 'A' && vkey <= 'Z')
    return true;
  if (vkey >= 'a' && vkey <= 'z')
    return true;
  if (vkey >= '0' && vkey <= '9')
    return true;
  if (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9)
    return true;
  return false;
}

inline void MarkShiftCombo() {
  g_shift_combo = true;
  g_block_ascii_sync_until = GetTickCount() + kAsciiSyncBlockMs;
}

inline void OnShiftKeyDown() {
  g_shift_armed = true;
  g_shift_combo = false;
  g_non_shift_key_seen = false;
  g_letter_key_seen = false;
}

inline void OnLetterOrDigitDuringShift(WPARAM vkey) {
  if (!IsLetterOrDigitVKey(vkey))
    return;
  if (g_shift_armed || (GetKeyState(VK_SHIFT) & 0x8000))
    g_letter_key_seen = true;
}

inline void OnComboKeyDown() {
  if (g_shift_armed) {
    g_non_shift_key_seen = true;
    MarkShiftCombo();
  }
}

inline void OnPhysicalShiftCombo() {
  if (GetKeyState(VK_SHIFT) & 0x8000) {
    g_non_shift_key_seen = true;
    MarkShiftCombo();
  }
}

// T-B3: Block shift toggle when combo is active or within 1.5s recovery window
// Does NOT clear state - caller uses g_* flags
inline bool ShouldBlockShiftToggle() {
  if (g_non_shift_key_seen || g_letter_key_seen)
    return true;
  // T-B3: 1.5s block window after shift combo (e.g. Shift+/ for Chinese punctuation)
  // prevents accidental toggle during ASCII sync recovery
  if (IsAsciiSyncRecentlyBlocked())
    return true;
  return false;
}

inline bool ShouldEatShiftRelease() {
  return ShouldBlockShiftToggle();
}

inline bool OnShiftKeyUpAfterCombo() {
  if (ShouldEatShiftRelease()) {
    g_block_ascii_sync_until = GetTickCount() + kAsciiSyncBlockMs;
    g_shift_armed = false;
    g_shift_combo = false;
    g_non_shift_key_seen = false;
    g_letter_key_seen = false;
    return true;
  }
  g_shift_armed = false;
  g_shift_combo = false;
  g_non_shift_key_seen = false;
  g_letter_key_seen = false;
  return false;
}

// Only called when Shift key is actually released (in key event handler)
inline void OnShiftKeyUpAlone() {
  g_shift_armed = false;
  g_shift_combo = false;
  g_non_shift_key_seen = false;
  g_letter_key_seen = false;
  g_block_ascii_sync_until = 0;
}

// Implementation of forward declaration
inline bool IsAsciiSyncRecentlyBlocked() {
  return GetTickCount() < g_block_ascii_sync_until;
}

inline bool ShouldIgnoreAsciiSync() {
  if (g_shift_armed || g_shift_combo)
    return true;
  return GetTickCount() < g_block_ascii_sync_until;
}

}  // namespace ShiftComboGuard
