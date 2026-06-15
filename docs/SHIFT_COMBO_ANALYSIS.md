# Shift 组合键误切换中英文 — 根因与试错记录

> 版本：0.18.33 | 更新：2026-06-11

**状态**：Shift 组合键误切换在 **0.18.16** 逻辑层已修复（阻断 `TrayCommand 40013/40014`）；若用户仍复现，优先检查 **System32 `weasel.dll` 是否已更新**（见 [FIX_ATTEMPTS.md](./FIX_ATTEMPTS.md) 0.18.20 实机根因）。

## 现象

在中文模式下按住 **Shift** 再按标点/符号（如 `Shift+,` 输入 `《`），松开 Shift 后输入法被切到英文；或任务栏语言栏显示与 Rime 实际模式不一致。

## 双层机制（必须同时理解）

| 层级 | 机制 | 说明 |
|------|------|------|
| **Rime** | `ascii_composer` + `switch_key` | Shift **释放**时若未检测到和弦（chord），执行 `commit_code` / `noop` 等 |
| **Windows TSF** | `GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION` | 部分按键不经 IME，系统直接翻转「转换模式」Compartment |

官方 Rime 思路（`librime/.../ascii_composer.cc`）：Shift 按下期间若有其它键经 IME 处理，则 `chord_key_pressed_=true`，释放 Shift **不**切换 — 与微软拼音同类。

**根本局限**：不经 TSF 的按键（部分标点直进应用）Rime 看不到，`chord_key_pressed_` 无法置位；同时 Windows 可能已改写 Compartment。

---

## 已证伪方案（勿再尝试）

| # | 方案 | 结果 | 原因 |
|---|------|------|------|
| 1 | `WH_KEYBOARD_LL` 全局低级钩子 | **失败** | 多进程重复安装 → 系统/托盘卡死，需强杀 WeaselServer |
| 2 | 仅用 TSF `pfEaten` 吞 Shift 释放 | **失败** | 标点不经 TSF 时 `g_shift_combo` 未置位 |
| 3 | 仅断 `INPUTMODE_CONVERSION` 反向同步 | **不足** | 日志仍见误切换；Rime 层 `commit_code` 仍会切 |
| 4 | `GetAsyncKeyState` 轮询 + 模拟 Enter（常用短语） | **失败** | Enter 泄漏到前台应用 |
| 5 | 托盘多次 `NIM_ADD` / 专用 GUID | **无关** | 曾致多图标，与 Shift 无关 |
| 6 | `KillOtherWeaselServerProcesses` 强杀 | **失败** | 多实例竞态 |
| 7 | `default.yaml` + 补丁脚本强制 `Shift_L/R: commit_code` | **加重问题** | 构建脚本 `patch-rime-ice-config.ps1` 写的是 **noop**；真正写 commit_code 的是 **`RemoveShiftNoopOverride()` + HotkeySettings**（0.18.49 前），已在 0.18.50 删除 |
| 8 | Shift 释放时 F13 重置 + `ShiftComboGuard` 吞键 | **部分有效** | 仅对经 TSF 的组合键有效；单独 Shift 或 bypass 键无效 |
| 9 | `ProcessKeyEvent` 硬回滚 ascii（除 Ctrl+Space/CapsLock） | **不足** | 0.18.15 日志：guard 已 block，但同秒 `SetOption` + tray `40013` 仍切换 |
| 10 | 启动强杀 / 反复 Shutdown 清进程 | **失败** | 0.18.13 假死；0.18.15 已移除 |
| 11 | Compartment 仅 ignore 反向同步 | **不足** | 须同时阻断 TrayCommand `40013/40014` → `SetOption` |
| 12 | 在旧 System32 `weasel.dll` 上继续改 TSF 逻辑 | **无效** | 新代码不加载，日志无 `weasel.dll loaded` / `ActivateEx` |

---

## 日志证实的第二条切换路径（0.18.15）

```
16:26:21 [ascii-guard] block ascii flip keycode=117
16:26:21 [set-option] SetOption ascii_mode=1 ipc_id=6
16:26:21 [ipc] post tray cmd id=40013 session=6
```

`ProcessKeyEvent` 与 `SetOption` 并发：后者来自 `ID_WEASELTRAY_ENABLE_ASCII`，与 Compartment 无关。

**0.18.16**：Server 拒绝无 `WEASEL_TRAY_USER_INITIATED` 的 `40013/40014`；用户切换改 `Client::SetAsciiMode()`。

**0.18.18+**：IPC 层直接丢弃未授权 `40013/40014`，不再 PostMessage 洪水（防假死）。

---

## 当前保留措施（0.18.16 – 0.18.33）

1. **Compartment**：忽略 `INPUTMODE_CONVERSION` 与 `KEYBOARD_OPENCLOSE` 的**反向**同步（`WeaselTSF/Compartment.cpp`）；Rime → 语言栏单向更新。
2. **TSF 层**：`ShiftComboGuard` + Shift 释放吞键（`KeyEventSink.cpp`），仅当 TSF 已识别组合键时生效。
3. **配置层**：
   - `default.yaml`：`Shift_L/R: noop`；`Control+space` → `toggle: ascii_mode`。
4. **Server 层（0.18.16）**：`TrayCommand 40013/40014` 须带 `WEASEL_TRAY_USER_INITIATED`；否则 `ascii-guard block` 并忽略。
5. **部署层（0.18.21+）**：`/upgrade` 强制更新 System32 `weasel.dll`；`force_silent` 升级也走 `install()`（0.18.22+）。

---

## 未实施 / 备选（按优先级）

| 方案 | 预期效果 | 代价 |
|------|----------|------|
| 启用 `TF_PRESERVEDKEY` + `VK_SHIFT` + `TF_MOD_ON_KEYUP`（上游注释代码） | TSF 官方路径拦截 Shift 释放 | 需实测各应用；可能与 key_binder 冲突 |
| 用户 `default.custom.yaml` 自定义 `switch_key` | 灵活 | 需文档说明 |
| 接受「Shift 单击不切换」，仅用 Ctrl+Space | **最稳** | 习惯改变 |

---

## 验证步骤

1. 确认 `weasel.tsf.log` 有 `weasel.dll loaded 0.18.33`（否则先执行 `/upgrade` 并注销）。
2. 中文模式，输入若干拼音但不选词。
3. `Shift+,` / `Shift+.` / `Shift+数字` 输入符号。
4. 松开 Shift：**应保持中文**；语言栏与候选窗状态一致。
5. `Ctrl+Space`：应切换中英文。
6. 查看 `weasel.server.log`：Shift 组合后应为 `ascii-guard block` 或 `ipc block`，不应出现成功的 `post tray cmd id=40013`。
7. 完整试错列表见 [FIX_ATTEMPTS.md](./FIX_ATTEMPTS.md)。

---

## 相关文件

- `WeaselTSF/KeyEventSink.cpp` — 按键与 ShiftComboGuard
- `WeaselTSF/Compartment.cpp` — Compartment 同步策略
- `include/ShiftComboGuard.h` — 状态机（无钩子）
- `output/data/default.yaml` — `ascii_composer.switch_key` / `key_binder`
- `WeaselSetup/WeaselSetup.cpp` — `/upgrade` 强制更新 System32 DLL
- `librime/.../ascii_composer.cc` — 官方和弦逻辑
