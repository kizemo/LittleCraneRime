# 小狼毫修复试错记录

> 更新：2026-06-14 | 当前版本：0.18.92（install-flow 加固版）

---

## 活跃总览（重构后）

### 目标与门控

| 门控 | 状态 |
|------|------|
| 快捷键 / shift-pick | **PASS**（T-B3 + S-B5，勿动） |
| 多宿主 TSF C++ 异常捕获 | L-TSF（0.18.91） |
| 候选窗可见（font≠0） | **PASS**（C-D13 style 灌入） |
| 微信连续输入不崩 | **PASS**（0.18.86） |
| 暂停后首键不崩 | C-D14 → **R-B1 待验** |
| 多宿主 / 重启服务 | 待验 |

### 架构（R-B1 重构后）

```
按键: Test(仅测键) → KeyDown(唯一 AfterRimeKey) → KeyUp(跳过已处理)
响应: Pull → idle(sync收尾) | compose(sync/async按策略)
状态: KeySinkState 单结构体；组字生命周期独立函数
```

### 试错预算（崩溃域）

| 项 | 上限 | 已用 | 剩余 |
|----|------|------|------|
| 崩溃分支 | 8（文档）/ 实际已扩 | ~14 | **2～3 轮定案** |
| 全局 | 12 | ~10 | 2 |
| 下一动作 | R-B1 验收 → FAIL 则 WinDbg | — | — |

### 假设树（精简）

| ID | 状态 | 版本 |
|----|------|------|
| C-D10 key-pull + sync 输入 | **证真** | 0.18.83 |
| C-D13 KeyUp 去重 + style | **证真（连续）** | 0.18.86 |
| C-D14 idle sync 收尾 | 并入 R-B1 | 0.18.87 |
| **R-B1** 按键/响应管线重构；去 coalesce | **收口待验** | 0.18.89 |
| U-B1 UI 生命周期 | 待试 | — |
| WinDbg 定栈 | 预算耗尽后 | — |

### 本轮：R-B1

- **保留**：T-B3、S-B5、key-pull、style 灌入、KeyUp 跳过、idle sync 收尾
- **简化**：`KeySinkState`；`_HandleIdleKeyResponse` / `_RequestKeyEditSession`；移除 async coalesce
- **验收**：连续输入 + 暂停 ≥2min 首键 + Cursor/微信交替

### 禁止重试

C-D2、C-D5～D9 全量门控/纯 async 无标志、S-B1/2、T-B1/2 — 见下文证伪表。

---

## 排查路线图（活跃 · 历史明细）

> 以下保留完整试错史；上方「活跃总览」为当前执行入口。

> 更新：2026-06-14 | 主问题：宿主崩溃（飞书/Cursor）| 基线：0.18.70

### 问题陈述

- **现象**：0.18.69 后飞书、Cursor 输入时崩溃（`0xE06D7363`）；Shift 快捷键冲突（**0.18.70 已解**）
- **影响范围**：Cursor、飞书、钉钉等 TSF 宿主
- **当前状态**：Shift **已解**；崩溃 **C-D3 证伪 → C-D4（0.18.72）待验收**

### 试错预算（可配置）

| 配置项 | 本问题设定 | 已用 | 剩余 |
|--------|------------|------|------|
| per_branch_max | 2 | — | — |
| per_domain_max（Shift） | 6 | 3 | 3 |
| per_domain_max（崩溃） | 8 | 2 | 6 |
| global_max | 12 | 6 | 6 |
| cross_verify_max | 3 | 0 | 3 |

### 假设树（完整路径）

| 分支 ID | 具体改动描述 | 可能性 | 状态 | 尝试版本 | 结论摘要 |
|---------|--------------|--------|------|----------|----------|
| S-B1 | `KeyEventSink`：`_ShouldAllowShiftCandidatePick` 门控放宽 | 高 | 已证伪 | 0.18.69 | `cc=0`+`combo=1` 恒阻断 |
| S-B2 | `LanguageBar`：有候选时 `_HasVisibleCandidateMenu` 禁 toggle | 中 | 已证伪 | 0.18.69 | 未解决选词 |
| S-B3 | `KeyEventSink`：组字期 Shift 隔离，IPC `shift-pick` 1/2 | 高 | **证真** | 0.18.70 | 用户确认快捷键 PASS |
| C-D1 | `dllmain`：UEF 写 `[crash] code=` | 低 | 已实施 | 0.18.68 | 仅诊断，不防崩 |
| C-D2 | `Deserializer`/`GetResponseData`/`_ProcessKeyEvent`：吞 IPC 异常，去 MessageBox | 高 | **已证伪** | 0.18.70 | 10:38/10:42 仍 `0xE06D7363` |
| C-D3 | `EditSession`：禁 sync 重入 + 延后 composition_dirty；IPC/UI try/catch | 高 | **已证伪** | 0.18.71 | 11:11/11:13/11:14 仍崩；非输入态亦崩 |
| C-D4 | `Deactivate`/`Disconnect` 加固 + `weasel.crash.log` 面包屑诊断 | 高 | **已证伪** | 0.18.72 | 微信 12:11:34；edit-session 风暴 |
| C-D5 | EditSession：IPC 刷新门控 + 200ms 节流 + 打断 flush 环 | 高 | **已证伪** | 0.18.73 | 闪退未解；Shift 复发（`_status.composing` 滞后） |
| S-B4 | `_IsActivelyComposing` + urgent IPC 豁免节流；shift-pick 同步刷新 | 高 | **已证伪** | 0.18.74 | Shift 选词修好但 idle Shift 无法切中英文/标点 |
| S-B5 | 收窄 `_ShouldIsolateShiftForComposing`；toggle 路径不再被宽判定阻断 | 高 | **证真** | 0.18.75 | 切换恢复；组字 shift-pick 仍 PASS |
| T-B1 | `_ToggleAsciiMode`：去乐观翻转；SetAsciiMode 兜底；启用 toggle tick 防抖 | 高 | **已证伪** | 0.18.76 | sync failed 完全阻断切换；SetAsciiMode 可能二次切换 |
| T-B2 | Toggle 后信任 IPC 成功+tick；去掉 SetAsciiMode 兜底；Reconnect 加固 | 高 | **已证伪** | 0.18.77 | 托盘可切但下一键回中文；Toggle 前 refresh 污染 ascii_before |
| T-B3 | SetAsciiMode 显式切换 + 去切换前 refresh；Server Respond；tick 保 full_shape | 高 | **证真** | 0.18.78 | 用户确认快捷键 PASS |
| C-D6 | EditSession 全局限流 + force 豁免 shift-pick；去按键双 refresh；IPC 异常吞掉 | 高 | **已证伪** | 0.18.79 | 微信 ~1min 仍崩；空转 IPC 风暴；零 throttled |
| C-D7 | 仅 eaten/组字时拉 IPC；12ms 最小间隔 + 18/200ms 限流 | 高 | **已证伪** | 0.18.80 | 微信仍崩；Cursor 候选窗不可见（style 被覆盖 font=0）；快捷键 PASS |
| C-D8 | 恢复每键拉 IPC；空闲节流+drain；idle-skip UI；布局用 _status.composing | 高 | **已证伪** | 0.18.81 | 快速双崩；Cursor `0xC0000005`；sync 风暴未减 |
| C-D9 | 按键线程立即 pull；空闲不进 ES；组字 async 合并 | 高 | **已证伪** | 0.18.82 | 第二字母卡死；候选窗不消失；`es async`+`skipped noop` |
| C-D10 | 保留 key-pull；非空闲一律 sync+设 refresh 标志；空闲 HideUI | 高 | **证真（输入）** | 0.18.83 | 组字/快捷键 PASS；多宿主切换仍崩 |
| M-B1 | 焦点切换：OnKillThreadFocus 异步收尾；idle 清 TSF 组字；focus 诊断 | 高 | **部分证伪** | 0.18.84 | 仅微信连续输入仍崩；多宿主待验 |
| C-D12 | 去 Test+Key 双路径；组字 async+coalesce；保留 key-pull/M-B1 | 高 | **部分证伪** | 0.18.85 | 微信 font=0 无窗；KeyUp 双 ES；重启服务双崩 |
| C-D13 | KeyUp 跳过已处理键；style 会话灌入；KeyPull 缓存 style | 高 | **部分证伪** | 0.18.86 | 连续输入 PASS；暂停后首键崩 |
| C-D14 | 空闲 sync 结束 TSF 组字；stale 状态复位；Reconnect 清组字 | 高 | 并入 R-B1 | 0.18.87 | 代码已写，未单独发版 |
| R-B1 | KeySinkState + 响应管线拆分；去 async coalesce | 高 | 进行中 | 0.18.88 | 待验收 |
| U-B1 | `WeaselUI`：绘制/窗口生命周期 | 低 | 待试 | — | — |

### 试错顺序（高 → 低）

1. S-B3（Shift）→ **PASS**
2. C-D2 → **FAIL**（证伪）
3. C-D3（EditSession 重入）→ 待确认
4. C-D4 → 预算内

**排序依据**：日志铁证 `0xE06D7363` + IPC 反序列化曾 MessageBox；Shift 与崩溃分域，Shift 用户优先。

### 本轮选定分支

- **ID**：R-B1（**保留** C-D13/C-D14 行为 + T-B3 + S-B5）
- **验收门控**：连续输入 + 暂停 ≥2min 首键 + 多宿主不崩
- **若 FAIL**：WinDbg；禁止再叠补丁，只加减已拆分模块

### 禁止重试清单

| 分支 ID | 具体改动描述 | 证伪版本 | 禁止原因 |
|---------|--------------|----------|----------|
| S-B1 | 门控放宽 `GetCount`+`ShiftComboGuard` 放行选词 | 0.18.69 | `shift-diag cc=0 allow=0` |
| S-B2 | 有候选禁 toggle 作为选词主方案 | 0.18.69 | 未产生 `shift-pick` |
| C-D2 | IPC 吞异常 + 去 MessageBox 作为崩溃主方案 | 0.18.70 | 10:38/10:42 仍崩 |

---

## 证伪登记表

### 证伪 — S-B1 @ 0.18.69

| 字段 | 内容 |
|------|------|
| **分支 ID** | S-B1 |
| **具体改动描述** | `KeyEventSink.cpp`：`_ShouldAllowShiftCandidatePick` 门控放宽 |
| **假设** | `allow=1` 时左/右 Shift 产生 `shift-pick` |
| **实际** | `shift-diag composing=1 cc=0 combo=1 block=1 allow=0` |
| **证伪结论** | UI `GetCount` 与 server 不同步，门控路线不可行 |
| **新证据** | 无 |
| **重试评估** | 不满足门禁 1–4 → **禁止重试** |

### 证伪 — S-B2 @ 0.18.69

| 字段 | 内容 |
|------|------|
| **分支 ID** | S-B2 |
| **具体改动描述** | `LanguageBar.cpp`：有候选时禁止 `_ToggleAsciiMode` |
| **假设** | 禁 toggle 可让 Shift 专用于选词 |
| **实际** | 仍无 `shift-pick`；选词与切换仍冲突 |
| **证伪结论** | 未解决选词路径，不能作为 Shift 主方案 |
| **新证据** | 无 |
| **重试评估** | 不满足门禁 1–4 → **禁止重试** |

### 证伪 — C-D2 @ 0.18.70

| 字段 | 内容 |
|------|------|
| **分支 ID** | C-D2 |
| **具体改动描述** | `Deserializer`/`GetResponseData`/`_ProcessKeyEvent` 吞异常；去 IPC MessageBox |
| **假设** | 宿主不再因未捕获 C++ 异常退出 |
| **实际** | `10:38:05`/`10:42:08` Cursor 仍崩；`[crash] code=0xE06D7363 addr=00007FFB84EA1B6A`；无 `unknown exception in DoEditSession` |
| **证伪结论** | 异常不在已包裹路径，或 sync 重入 EditSession 导致 |
| **新证据** | 新 dmp 时间戳；日志无 DoEditSession catch 行 |
| **重试评估** | 不满足门禁 1–4 → **禁止重试** |

---

## 日志定位结论（2026-06-12，0.18.15 实机）

### 进程与双图标

| 观测 | 结论 |
|------|------|
| 任务管理器仅 1 个 `WeaselServer.exe`（如 PID 13796） | **不是**两个算法服务进程并存 |
| `weasel.tray.log` 多次 `Create NIM_ADD` + `NIM_DELETE ok=0` + 再次 `NIM_ADD` | **同一进程**注册多个托盘 notify icon；删除失败留下幽灵图标 |
| 各次启动 `hwnd` 不同（如 `0x1501D2` → `0x1033E`） | 服务重启后旧 hwnd 上的图标无法被新进程清理 |
| 图标 id 从 `105` 变为 `24144`（`0x5E50`） | 历史 id 的孤儿图标仍可能显示 |

**根因**：`CSystemTray::Create`（`SYSTEMTRAY_USEW2K` 下先 `NIM_ADD`）+ `WeaselTrayIcon::Create` 再调 `AddIcon()` 二次注册；`NIM_DELETE` 对尚未存在的条目返回失败。

### Shift 误切换

| 观测 | 结论 |
|------|------|
| `weasel.ascii.log` 有 `[ascii-guard] block ascii flip` | `ProcessKeyEvent` 硬拦截**曾生效** |
| 同秒出现 `[set-option] SetOption ascii_mode=1` + `weasel.server.log` `post tray cmd id=40013` | 切换仍经 **TrayCommand → SetOption** 绕过 guard |
| `weasel.tsf.log` 大量 `flush tray cmd id=40013/40014` | 旧版/残留 TSF 路径经 `_FlushPendingTrayCommand` 发 ascii 托盘命令 |
| `compartment` 已 `ignore INPUTMODE_CONVERSION` 但仍见 tray cmd | Compartment 反向同步已关，**TrayCommand 40013/40014 是独立第二条路径** |

**根因**：`ascii-guard` 只挡 `process_key`；`ID_WEASELTRAY_ENABLE/DISABLE_ASCII` 直接 `SetOption(ascii_mode)` 无授权校验。

### 删除候选

| 观测 | 结论 |
|------|------|
| `weasel.server.log` 无 `DeleteCandidate` 记录 | 右键菜单未走到 IPC，或会话未建立 |
| 候选窗 `WS_EX_NOACTIVATE` | `TrackPopupMenu` 需 `AttachThreadInput` 才能可靠弹出 |

---

## 0.18.12 – 0.18.15 已尝试（无效或不足）

| 版本 | 方案 | 结果 | 日志/原因 |
|------|------|------|-----------|
| 0.18.12 | `ProcessKeyEvent` 回滚非 Ctrl+Space 的 ascii 变更 | 不足 | `SetOption` 仍改写模式 |
| 0.18.12 | 候选删除 `RequestEditSession`、去 `WS_EX_TRANSPARENT` | 删除仍无效 | 菜单未弹出，无 IPC 日志 |
| 0.18.13 | 启动 `KillOtherWeaselServerProcesses` + 反复 Shutdown | 假死/竞态 | 已移除 |
| 0.18.13 | `PurgeProcessTrayIcons` + GUID 托盘 | 仍双图标 | 未清 GUID 全局孤儿；二次 `NIM_ADD` |
| 0.18.14 | `TF_PRESERVEDKEY` + Shift | 未单独验证 | 与 tray cmd 路径并存 |
| 0.18.14 | 部署修补 `default.yaml` Shift noop | 不足 | 切换不经 Rime 按键 |
| 0.18.15 | 移除启动强杀；TSF 60s 内最多拉起服务一次 | 双图标/Shift 仍在 | 见上日志 |
| 0.18.15 | 除 Ctrl+Space/CapsLock 外禁止按键切 ascii | **仍无效** | `40013/40014` 绕过 |
| 0.18.15 | 托盘左键去掉 wParam 检查 | 快捷栏部分改善 | 与 Shift/双图标无关 |

---

## 已解决（用户确认 0.18.16，0.18.17 回退）

| 问题 | 结论 |
|------|------|
| Shift 组合键误切中英文 | **0.18.16 已解决** — 阻断未授权 `TrayCommand 40013/40014` + `ProcessKeyEvent` ascii-guard |
| 任务栏双托盘图标 | **0.18.16 已解决**；0.18.17 用户反馈复发（见下） |

---

## 0.18.16 修复（基于日志）

1. **ascii 授权位**：`WEASEL_TRAY_USER_INITIATED`；仅 `Client::SetAsciiMode()`（语言栏/CLI）可改 `ascii_mode`；未授权 `40013/40014` 在 Server 记 `ascii-guard block` 并忽略。
2. **TSF**：`_FlushPendingTrayCommand` 丢弃遗留 ascii tray cmd；用户切换改用 `SetAsciiMode`。
3. **托盘**：`PurgeWeaselTrayIconByGuid()`；`Create` 用 `ShowIcon` 替代重复 `AddIcon`；`AddIcon` 前按 GUID 全局删除。
4. **删除**：`AttachThreadInput` + `TrackPopupMenuEx`；Server/TSF 增加 `delete` 日志。

---

## 验证清单（0.18.16 安装后）

1. 安装后**注销或重启**（确保 `WeaselTSF.dll` 重新加载）。
2. `%TEMP%\rime.weasel\weasel.server.log`：Shift 组合后应见 `ascii-guard block tray ascii cmd`，**不应**再出现未 block 的 `post tray cmd id=40013`。
3. `weasel.tray.log`：每次启动仅一组 `NIM_ADD`（无连续 `ok=0` 的 DELETE 后立即双 ADD）。
4. 右键删词：`weasel.server.log` 出现 `[delete] DeleteCandidate ... ok=1`。
5. 任务栏托盘图标：仅一个小狼毫图标（若仍有幽灵图标，注销一次 Windows）。

---

## 0.18.17 修复（删除候选 + 溢出区幽灵图标）

### 日志结论（删除候选仍无效）

| 观测 | 结论 |
|------|------|
| 0.18.16 安装后 `weasel.server.log` / `weasel.ui.log` 仍无 delete 记录 | `TrackPopupMenu` 在 `WS_EX_NOACTIVATE` 分层候选窗上无法弹出 |
| 左键点选候选正常 | 鼠标可达面板；问题在系统菜单而非 IPC |

### 修复

1. **内嵌删除条**：右键候选显示「删除该候选词」条，左键确认后 IPC 删除（`weasel.ui.log` 全链路）。
2. **溢出区多「小狼毫算法服务」**：`PurgeAllLegacyWeaselTrayIcons()`（GUID + 历史 uID）；安装时 `/purge-tray`；`Create` 仅在可见时 `NIM_ADD`。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| `AttachThreadInput` + `TrackPopupMenuEx`（0.18.16） | 日志仍无 delete，用户无反应 |
| 内嵌删除条 + `PurgeLegacy` 无 hwnd（0.18.17） | `weasel.ui.log` 不存在；`PurgeLegacy ok=0` 无法清注册表；双图标/假死复发 |

---

## 0.18.17 回归日志结论（2026-06-12 17:07+）

| 观测 | 结论 |
|------|------|
| **`weasel.ui.log` 不存在** | 新 WeaselUI/面板代码未加载（`weasel.dll` 未随 ctfmon 重载）或右键从未到达面板 |
| `weasel.tsf.log` 仍有 `flush tray cmd id=40013` | 内存中 TSF 仍为旧逻辑；compartment 仍触发 pending tray |
| `weasel.server.log` 17:11:28 **每秒数十条** `post tray cmd 40013/40014` | IPC 层仍 PostMessage 到 UI 线程，虽被 block 但**消息洪水导致假死** |
| `PurgeLegacy id=* ok=0` 全部失败 | `Shell_NotifyIcon` 无 hwnd 无法清除；溢出区 15+ 项在 **NotifyIconSettings 注册表** |
| 任务栏 **两个图标** | 除 WeaselServer 托盘外，**语言栏 `TF_LBI_STYLE_SHOWNINTRAY`** 也注册托盘图标 |

---

## 0.18.18 修复

1. **假死**：IPC 层直接丢弃未授权 `40013/40014`，不再 PostMessage 洪水。
2. **双图标**：移除 `TF_LBI_STYLE_SHOWNINTRAY`（仅保留 WeaselServer 托盘）。
3. **溢出区 15+ 项**：清理 `NotifyIconSettings` 中 `ExecutablePath` 含 `WeaselServer.exe` 的项；安装时 ctfmon 重启后再 `/purge-tray`。
4. **删除候选**：线程 `WH_MOUSE` 钩子转发右键到面板；`Ctrl+Delete` 删除高亮候选；`weasel.ui.log` 全链路。
5. **TSF 版本**：`ActivateEx` 写 `WeaselTSF x.x.x` 到 `weasel.tsf.log` 便于确认 DLL 已重载。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅 `Shell_NotifyIcon` PurgeLegacy（无 hwnd） | 全部 `ok=0`，溢出区设置项不减少 |
| UI 线程 block 40013 但仍 IPC PostMessage | 假死复发 |
| 内嵌删除条仅靠面板 WM_RBUTTON*（0.18.17） | 无 `weasel.ui.log`，右键无反应 |

---

## 0.18.18 回归日志结论（2026-06-12 17:33–17:46）

| 观测 | 结论 |
|------|------|
| **`weasel.ui.log` 仍不存在** | `weasel.dll` 未重载（ctfmon/TextInputHost 仍持旧 DLL） |
| **`weasel.tsf.log` 无 `ActivateEx` / `weasel.dll loaded`** | 同上；0.18.18 TSF 代码未生效 |
| `weasel.tsf.log` 仍有 `flush tray cmd id=40013` | 旧 compartment 逻辑仍在发 tray 命令 |
| `weasel.tray.log` 每次启动仅 **1 次** `NIM_ADD` | 双图标**不是**同进程二次注册 |
| `PurgeNotifyIconSettings` 无 `removed=N` 日志 | 父键以 `KEY_READ` 打开，`RegDeleteKeyW` 全部失败 |
| 任务栏两个「中」图标 | **WeaselServer 托盘** + **未重载的旧 TSF 语言栏托盘**（或历史幽灵项） |
| 设置中 10 个「小狼毫算法服务」 | `NotifyIconSettings` 未清理成功 |

---

## 0.18.19 修复

1. **注册表清理**：`NotifyIconSettings` 以 `KEY_WRITE` 打开，用 `RegDeleteTreeW`；匹配 `ExecutablePath` / `InitialTooltip`；记录 matched/removed。
2. **双图标**：`display_tray_icon: true` 时隐藏 TSF 语言栏按钮（`_SyncLanguageBarTrayVisibility`）；`OnActivated` 不再强制 `Show(TRUE)`。
3. **DLL 重载**：`DllMain` 写 `weasel.dll loaded x.x.x`；`/reload-tsf`；安装流程改为 quit → purge → **reload-tsf** → WeaselSetup → purge。
4. **删除候选**：`Shift+Ctrl+1`…`9` 删除第 1–9 位候选（不依赖面板右键）；保留 `Ctrl+Delete` 删高亮项。
5. **日志**：IPC block 限流（每秒最多 1 条 + suppressed 计数）。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 移除 SHOWNINTRAY 但不重载 weasel.dll（0.18.18） | 双图标、右键删除仍无效 |
| `NotifyIconSettings` 只读打开父键删除子键（0.18.18） | removed=0，设置项不减少 |
| 仅靠安装脚本 taskkill ctfmon（0.18.18） | 用户机仍无 ActivateEx，DLL 未换 |

---

## 0.18.20 审查修补

1. **日志**：`_SyncLanguageBarTrayVisibility` 仅在可见性变化时写日志；UI/IPC ascii block 共用限流。
2. **删除快捷键**：支持小键盘 `Shift+Ctrl+1`…`9`；鼠标钩子在 HWND 变化时重装。
3. **样式**：`UpdateStyle` 同步 `_style` 与 `_ui->style()`。
4. **注册表清理后**广播 `WM_SETTINGCHANGE`/`TraySettings` 刷新托盘设置 UI。
5. **`/reload-tsf`**：`CreateProcessW` 使用可写命令行缓冲区。

---

## 0.18.20 实机根因（2026-06-12 18:07，日志+注册表+文件交叉验证）

| 证据 | 结论 |
|------|------|
| `System32\weasel.dll` 1068032B @00:39 ≠ 安装目录 `weaselx64.dll` 1084416B @18:00 | **TSF 加载的 System32 DLL 从未更新** |
| `weasel.tsf.log` 无 `weasel.dll loaded` / `ActivateEx`；仍有 `flush tray cmd id=40013` | 内存中 TSF 为**旧 DLL**（新代码应 `drop legacy`） |
| `weasel.ui.log` 不存在 | 删除快捷键/右键/面板代码**均未执行** |
| `WeaselServer` 来自 `weasel-0.18.20` 目录 | 仅 Server 更新，TSF 未更新 |
| `CustomInstall`：`has_installed()` 为真时**跳过 `install()`** | NSIS `/i /setupquiet` 升级时**不拷贝 DLL 到 System32** |
| `PurgeNotifyIconSettings open failed` | 代码路径 `Explorer\NotifyIconSettings` **不存在** |
| 实机幽灵项在 `HKCU\Control Panel\NotifyIconSettings`（12+ 条） | 托盘设置项清理路径**写错** |

**单一根因**：升级时 `WeaselSetup` 不更新 `C:\Windows\System32\weasel.dll` → 所有 TSF 侧修复（删除、语言栏、快捷键）均不生效；托盘幽灵项因注册表路径错误未清理。

---

## 0.18.21 修复（经独立审查 REVISE→实施）

1. **`WeaselSetup /upgrade`**：无论是否已安装，强制 `install()`（拷贝 System32 + regsvr32）；拷贝后校验 size 一致，失败返回 1。
2. **`install.nsi`**：删除早期 `/setupquiet`（会 skip install）；顺序 quit → kill TSF 宿主 → `/upgrade` → `/reload-tsf` → `/purge-tray`。
3. **托盘注册表**：同时清理 `Control Panel\NotifyIconSettings` 与 `Explorer\NotifyIconSettings`。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅 kill ctfmon / `/reload-tsf` 不更新 System32 DLL | TSF 仍加载旧 weasel.dll，一切 TSF 修复无效 |
| `WeaselSetup /i /setupquiet` 升级 | `has_installed` 跳过 `install()`，DLL 不拷贝 |
| 仅改 Explorer 路径清理 NotifyIconSettings | Win11 26200 实机项在 Control Panel 路径 |
| 在旧 DLL 上继续改 KeyEventSink/面板/钩子 | 代码不执行，无日志 |

---

## 0.18.22 – 0.18.32 加固（另一台电脑同步合并）

在 0.18.21 根因修复之上继续迭代，**不再新增 TSF 删除/托盘逻辑**，重点加固升级链路与运行时清理。

1. **`CustomInstall` 双保险**：`if (!_has_installed || force_silent)` 时强制 `install()`，避免 `/i /setupquiet` 升级仍跳过 System32 拷贝。
2. **`install.nsi` 增强**：
   - 升级前 `taskkill WeaselServer.exe`（释放文件锁）
   - `/upgrade` 失败 → `MessageBox` + `Abort`（不再静默失败）
   - 写入 `install.log` 并提示检查 `weasel.tsf.log` 中 `weasel.dll loaded` 版本
3. **托盘运行时清理拆分**：`PurgeLightTrayIcons()`（GUID + 进程内）用于日常启动；完整注册表清理仍由 `/purge-tray` 调用 `PurgeAllLegacyWeaselTrayIcons()`。
4. **验收脚本**：`scripts/accept_test.ps1` 用于管理员手动执行 `/upgrade` 并比对 System32 DLL 大小。

### 验收门控（0.18.32，必须通过才宣称修复）

| 日志/检查项 | 期望 |
|-------------|------|
| `install.log` | `verify System32 weasel.dll size=... ok=1` + `install() success` |
| `weasel.tsf.log` | `weasel.dll loaded 0.18.32` + `WeaselTSF 0.18.32 ActivateEx`；**无** `flush tray cmd id=40013` |
| `weasel.tray.log` | `PurgeNotifyIconSettings base=Control Panel\... matched=N removed=N` |
| 任务栏 | 仅 1 个小狼毫图标；设置中「小狼毫算法服务」≤1 项 |
| 删除候选 | `Shift+Ctrl+1` → `weasel.ui.log` delete + `server.log` `[delete] ok=1` |

### 手动修复（管理员，已装包但 TSF 未更新时）

```bat
taskkill /f /im TextInputHost.exe & taskkill /f /im ctfmon.exe
"WeaselSetup.exe" /upgrade
"WeaselServer.exe" /reload-tsf
"WeaselServer.exe" /purge-tray
```

然后注销或重启。

---

## 0.18.33 审查修复（多代理验收）

三代理（Bugbot / Security Review / Code Reviewer）审查 0.18.32 相对 0.17.4 的全部修改后，本版修复以下阻塞项：

1. **IPC 安全**：`RequestCommitText` 改用 `WM_COPYDATA` 传 UTF-16 文本，TSF 侧校验发送方 PID 为 `WeaselServer.exe`；移除跨进程堆指针 `delete`。
2. **删除短语**：`RemoveCustomPhraseEntry` 在 `code` 为空时不再首轮删除所有同形词条，仅保留 `phrase_hits==1` 唯一匹配逻辑。
3. **ARM64 升级校验**：`verify_system32_weasel_dll` 在 ARM64 机器比对 `weaselARM64X.dll` 而非 `weaselx64.dll`。
4. **词典管理**：`SaveEntries` 按 `custom_phrase.txt` / `custom_phrase_double.txt` 分文件写入。
5. **维护模式**：`StartMaintenance` / `EndMaintenance` 清空 `m_client_notify_hwnd`；`SetOption` 在无活跃会话时跳过假会话写入。
6. **UI 几何记忆**：窗口坐标 `x/y=0` 视为合法位置。
7. **t9 方案**：`custom_phrase.db_class` 改为 `tabledb` 以支持右键删除。

### 验收门控（0.18.33）

| 日志/检查项 | 期望 |
|-------------|------|
| `install.log` | `verify System32 weasel.dll size=... ok=1` + `install() success` |
| `weasel.tsf.log` | `weasel.dll loaded 0.18.33` + `WeaselTSF 0.18.33 ActivateEx` |
| 词典管理 | 编辑双拼词条后 `custom_phrase_double.txt` 不被清空 |
| 常用短语面板 | 左上角 (0,0) 位置可正确恢复 |

---

## 0.18.45 – 0.18.48 未解决（用户实机验收 FAIL，2026-06-11）

用户安装 0.18.48 后确认以下三项**仍未解决**：

1. **单字母无候选**（需第二键才出候选）
2. **中文模式 AI+回车 → A4I**（期望 AI）
3. **Shift 切英文后首键回中文**（托盘曾显示 EN）

### 根因（多代理 + 日志 + 上游对比）

| 问题 | 根因 | 为何 0.18.45–48 无效 |
|------|------|----------------------|
| 首键无候选 | fork 偏离上游：`OnTestKeyDown` **未**调用 `_UpdateComposition`；IPC 单一 buffer，UI 刷新推迟到 `OnKeyDown` 后与异步 EditSession 竞态 | 0.18.47–48 的 `TF_ES_SYNC`、`_rime_key_dispatched` 只修补 KeyDown 路径，未恢复「TestKeyDown 后立即消费响应」 |
| AI→A4I | 双阶段按键 + 陈旧 pipe buffer；历史 F13 污染（0.18.47 前） | 0.18.48 移除 F13 但未保证「一物理键 = 一次 ProcessKeyEvent + 一次 GetResponseData」 |
| Shift 回中文 | `SetAsciiMode` 后 `GetResponseData` 可能读到**切换前**的 buffer，`_status.ascii_mode` 被覆盖 | 0.18.48 同步 SetOption 正确，但 DoEditSession 仍可能应用陈旧 status |

### 0.18.49 方案（对齐上游 WeaselTSF + WeaselIME 模式）

1. **KeyEventSink**：`OnTestKeyDown`/`OnTestKeyUp` 在 `ProcessKeyEvent` 后恢复 `_UpdateComposition`（与上游 master 一致）；`OnKeyDown` pending **不再**重复 `_UpdateComposition`；非 pending 路径用异步 `_UpdateComposition`（与上游一致）。
2. **EditSession**：Shift 切换后 1.5s 内若 GetResponseData 的 `ascii_mode` 与用户切换目标不一致，保留用户切换值（防陈旧 buffer 覆盖）。
3. **保留**：`_rime_key_dispatched` 防双送键、`ShiftComboGuard`、F13 禁用、SetAsciiMode 同步 IPC。

### 证伪（0.18.45–48，勿再试）

| 方案 | 结果 |
|------|------|
| 仅 KeyDown pending + `TF_ES_SYNC` 刷新，TestKeyDown 不刷新 | 用户 0.18.48 仍首键无候选 |
| 按键前 F13 `ProcessKeyEvent` 重置 latch | 覆盖 IPC buffer → AI→A4I |
| `_ToggleAsciiMode` 后立刻 `_UpdateComposition` 拉 status | SetOption 不写 pipe buffer → 读到更旧响应，加重 ascii 回退 |
| 全局移除 TestKeyDown `_UpdateComposition`（仿 #983） | 与 Excel/首键修复（CHANGELOG 0.16.0）及上游 master 相反 |

### 验收门控（0.18.49）

| 检查项 | 期望 |
|--------|------|
| `weasel.tsf.log` | `weasel.dll loaded 0.18.49` |
| 中文单按 `n` | **首键即出候选** |
| 中文 `AI` + Enter | 上屏 `AI`（非 `A4I`） |
| Shift 切 EN → 输入字母 | 托盘与输入均保持 EN |
| 回归 | Ctrl+Space、Shift+标点、常用短语、单托盘 |

---

## 0.18.49 实机验收 FAIL（2026-06-11，用户反馈）

### 现象

| 阶段 | 结果 |
|------|------|
| 安装 + 重启（**未** redeploy） | 首字母候选 **正常** |
| 按说明执行 **重新部署** | 三项问题 **全部复现**：首键无候选、Ctrl+Space/Shift 无法切换中英文 |

### 根因（配置层，非 TSF DLL）

**`Configurator::RemoveShiftNoopOverride()`** 在每次 `WeaselDeployer /deploy` 末尾执行，把用户 `default.yaml` 中 `Shift_L/R: noop` **改回 `commit_code`**，并通过 `weasel_hotkeys.yaml` patch 注入 Rime 层切换，与 0.18.49 设计（**TSF `_ToggleAsciiMode` + Rime noop**）直接冲突。

| 文件 | 破坏性逻辑 |
|------|------------|
| `Configurator.cpp` L43–100（旧） | `RemoveShiftNoopOverride()`：noop→commit_code |
| `HotkeySettingsDialog.cpp` L167–170（旧） | `ascii_action` 对 Shift 写 **commit_code** |
| `patch-rime-ice-config.ps1` | 构建时写 **noop**（正确），被 deploy hook **覆盖** |

Redeploy 还触发 `StartMaintenance` → Rime `finalize`/`initialize`，session 重建后 IPC 时序更敏感，配置错误叠加后表现为「全部失效」。

### 0.18.49 方案为何看似有效又失效

- 安装后 TSF 新 DLL 已加载，shared `default.yaml` 仍为 noop → **暂时正常**
- Redeploy 写入 commit_code 并重编 `%AppData%\Rime\build\default.yaml` → **全面回退**

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅改 KeyEventSink / EditSession（0.18.49）不修 deploy hook | 安装后正常，**redeploy 后必崩** |
| 让用户 redeploy 使配置生效 | **反而触发** RemoveShiftNoopOverride，与目标相反 |
| `RemoveShiftNoopOverride` + hotkeys commit_code | 与 `patch-rime-ice-config.ps1`、TSF Shift 切换 **三重冲突** |

### 0.18.50 方案

1. **删除** `RemoveShiftNoopOverride()`，改为 **`EnsureDeployAsciiConfig()`**：强制 Shift **noop**、补 `Control+space` key_binder。
2. Deploy 顺序：修配置 → deploy → weasel.yaml → 再修 → **二次 deploy** 确保 `build/` 正确。
3. `HotkeySettingsDialog::ascii_action`：Shift 返回 **noop**；patch 写入 `Control+space` toggle。
4. **TSF 层** `Ctrl+Space` → `_ToggleAsciiMode()`（不依赖 yaml）。
5. `OnTestKeyDown`：`ProcessKeyEvent` 后 **`_UpdateComposition(sync)`** 立即消费 IPC（maintenance 后首键）。

### 验收门控（0.18.50）

| 检查项 | 期望 |
|--------|------|
| **redeploy 后** `%AppData%\Rime\build\default.yaml` | `Shift_L/R: noop` |
| redeploy 后 | Ctrl+Space、Shift 单击均可切换 |
| redeploy 后 | 中文单按 `n` 首键出候选 |
| `weasel.tsf.log` | `weasel.dll loaded 0.18.50` |

---

## 0.18.51 方案（安装一站式收尾）

### 问题

0.18.50 **未做安装验收**；旧 `install.nsi` 在 Server 未启动时执行 `/deploy`、不检查退出码，Finish 页仍提示注销/重启或手动 redeploy（0.18.49 实测 redeploy 会触发配置破坏）。

### 根因（安装编排层）

| 缺口 | 影响 |
|------|------|
| deploy 在 Server 启动**之前** | maintenance 模式不完整，首 deploy 可能失败 |
| 不检查 `/deploy` 退出码 | 失败仍显示安装成功 |
| NSIS 与 WeaselSetup 步骤分散 | 难以原子验收 |
| Finish 页要求重启/redeploy | 与用户期望冲突；redeploy 曾触发 `RemoveShiftNoopOverride`（0.18.50 已修） |

### 0.18.51 实施

1. **`WeaselSetup /postinstall`**：写 Run 键 → 启动 Server → `/deployquiet`（最多 5 次重试）→ `/reload-tsf` → `/purge-tray` → 再启 Server → **System32 大小校验** → 可选等待 `weasel.tsf.log`（软检查，不阻塞）。
2. **`WeaselDeployer /deployquiet`**：`UpdateWorkspace(false)` 静默部署。
3. **`install.nsi`**：仅 `/upgrade` + `/postinstall`，失败 Abort；Finish 文案改为「无需注销、重启或手动 redeploy」。
4. 日志统一写入 `%TEMP%\rime.weasel\install.log`。

### 正确安装顺序（勿回退）

```
/upgrade → /postinstall(启Server→deployquiet→reload-tsf→purge→verify)
```

**禁止**再让用户「安装后必须 redeploy」作为修复步骤。

### 验收门控（0.18.51）

| 检查项 | 期望 |
|--------|------|
| 安装完成（无手动操作） | Ctrl+Space、Shift、首键候选均正常 |
| `install.log` | `postinstall() success` |
| System32 `weasel.dll` | 大小与安装目录一致 |
| `%AppData%\Rime\build\default.yaml` | `Shift_L/R: noop`（deploy 已内嵌 `EnsureDeployAsciiConfig`） |
| `weasel.tsf.log` | 首次使用输入法后出现 `weasel.dll loaded 0.18.51`（软门控） |

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| NSIS 直接 `/deploy` 且不查退出码 | 失败静默，用户以为装好了 |
| 安装后提示用户 redeploy | 0.18.49 实测有害；0.18.50 虽修配置层，仍增加不必要步骤 |
| postinstall 硬等 `weasel.tsf.log` 才成功 | DLL 仅在应用首次加载 IME 时写入日志，会导致误报失败 |
| 二次启动 Server 前不 `/quit` | 可能短暂双实例；0.18.51 已在第二次启动前 quit |

---

## 0.18.51 验收失败（2026-06-13 实机）

### 用户反馈

- 原三项问题**依旧**
- **新回归**：Ctrl+C、Ctrl+V 等系统快捷键全部失效
- 对「代码审查 PASS」不满：审查未覆盖实机行为

### install.log 铁证（18:05 安装）

```
[postinstall] deploy attempt=1 exit=0
[postinstall] reload-tsf exit=0
[setup] verify System32 ok=1 src=...\weasel-0.18.51\weaselx64.dll
[setup] System32 weasel.dll version=0.18.49.0   ← 版本仍是旧版！
[postinstall] postinstall() success              ← 误报成功
```

**部署（Rime config）成功**，但 **System32 TSF DLL 实际未更新到 0.18.51**（仅比对了文件大小，0.18.49 与 0.18.51 恰好同 size=1090048）。

### 根因 A：Ctrl+C/V 新回归（TSF 代码，0.18.49 起）

`KeyEventSink.cpp` 在 `_rime_key_dispatched=TRUE` 但 `*pfEaten=FALSE`（Rime 对 ctrl+key 返回 noop）时仍设 `_fTestKeyDownPending=TRUE`；Word 等应用重复 `OnTestKeyDown` 时 L285 硬返回 `*pfEaten=TRUE`，吞掉 Ctrl+C/V。

上游仅当 `*pfEaten` 时设 pending；重复 TestKeyDown 对未 eaten 键不吞。

### 根因 B：原三项问题仍在（TSF DLL 未真正加载）

| 证据 | 含义 |
|------|------|
| `System32 version=0.18.49.0` | 新 TSF 代码（TestKeyDown sync、Ctrl+Space TSF 切换）**未运行** |
| 无 `weasel.tsf.log` | ctfmon 未加载新 DLL |
| `build/default.yaml` Shift noop 正确 | 配置层 0.18.50+ 修复**已生效**，非主因 |

### 0.18.51 审查为何误 PASS

| 遗漏 | 后果 |
|------|------|
| 未对照上游 pending 条件（`dispatched` vs `eaten`） | Ctrl+C/V 回归未检出 |
| 未要求实机 `install.log` 版本行验收 | size-only verify 误通过 |
| 未验证 `System32` FileVersion == 安装包版本 | 以为装好了，实际旧 DLL |

### 0.18.52 方案

1. **KeyEventSink**：pending **仅** `*pfEaten`；重复 TestKeyDown 返回 `_fTestKeyEaten`（非硬 TRUE）。
2. **verify_system32**：**大小 + FileVersion** 必须与 `weaselx64.dll` 一致。
3. **postinstall**：内嵌 `/upgrade` 再校验；版本不匹配则失败（exit 14），不再误报 success。
4. 审查门控新增：必须检查 `install.log` 中 `src_version` == `System32 version`。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| `_rime_key_dispatched` 即设 pending | **导致 Ctrl+C/V 失效**（0.18.51 实测） |
| System32 仅比文件大小 | 0.18.49/0.18.51 同 size 误通过 |
| 代码审查不读实机 install.log | 连续误 PASS |

---

## 0.18.52 验收（2026-06-13，用户反馈）

### 结果

| 项 | 结果 |
|----|------|
| Ctrl+C / Ctrl+V | **已修复**（pending 仅 `*pfEaten`） |
| 单字母无候选 | **仍未修复** |
| Shift / Ctrl+Space 切换 | **仍未修复** |

### install.log / tsf.log 铁证

- 0.18.52 安装成功：`System32 version=0.18.52.0 src_version=0.18.52.0`
- `weasel.tsf.log`：`WeaselTSF 0.18.52 ActivateEx`（DLL 已真正加载）
- `weasel.ascii.log`：19:34 后 **11 次 `SetOption ascii_mode=1`，0 次 `ascii_mode=0`**
- `shift-guard`：`shift release eaten=1 tsf_combo=1`（Shift 单击被吞，未进 `_ToggleAsciiMode`）

### 根因（运行时状态，非安装/配置）

| 问题 | 根因 |
|------|------|
| 无法切回中文 | `_ToggleAsciiMode()` 用**过时的** `_status.ascii_mode`（常为 0）取反 → 反复 `SetOption ascii_mode=1`；Server 已在英文，TSF 以为在中文 |
| Shift 无效 | `OnPhysicalShiftCombo()` 把 **Ctrl/Alt/Space 等修饰键** 在 Shift 按住时标为 combo → `ShouldEatShiftRelease()` 吞掉 Shift 释放 |
| 单字母无候选 | `ascii_mode=1` 时字母直出英文，不走拼音候选（与切换失败同源） |

配置层 `D:\Users\Duanyi\rime\build\default.yaml`：`Shift noop` + `Control+space` **正确**（非 redeploy 破坏）。

### 0.18.53 方案（对齐 0.18.49 曾生效路径 + 日志驱动修补）

1. **`_QueryAsciiModeForToggle()`**：切换前从 IPC buffer / TSF compartment 读取**真实** ascii 状态，再取反调用 `SetAsciiMode`。
2. **`SetOption(ascii_mode)` 后 `_Respond()`**：把最新 status 写入 pipe，供 TSF `GetResponseData` 同步。
3. **Server `ToggleOption(ascii_mode)` + pipe `_Respond`**：`ID_WEASELTRAY_TOGGLE_ASCII` 从 Rime `get_option` 取反，写入 pipe 供 TSF 同步。
4. **Shift guard**：`IsPhysicalShiftComboKey()` 排除 Ctrl/Alt/Space/Shift；单击切换仅检查 `g_shift_combo`；Ctrl+Space 前清除 guard。
5. **移除** `StartSession` 的 `restore_ascii`。

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅修安装/verify 不修 `_ToggleAsciiMode` 状态源 | 0.18.52 DLL 已加载仍 FAIL |
| `_ToggleAsciiMode` 基于 `_status` 直接取反 | 日志证伪：永远 `ascii_mode=1` |
| `ShouldEatShiftRelease()` 阻止 Shift 单击切换 | 日志证伪：combo 误标 |
| 让用户手动 redeploy | 0.18.49 实测有害 |

---

## 0.18.53 验收（2026-06-13，用户反馈）

### 结果

| 项 | 结果 |
|----|------|
| Ctrl+C / Ctrl+V | **保持正常** |
| 单字母无候选 | **仍未修复** |
| Shift / Ctrl+Space 切换 | **安装后仍无效；手动 redeploy 后可切换** |
| 语言栏图标 | **切换后滞后 1–2 键才显示正确中/EN** |

### install.log / tsf.log / ascii.log 铁证（0.18.53）

- 安装成功：`System32 version=0.18.53.0`
- `postinstall deploy attempt=1 exit=0`（20:05:31）→ `reload-tsf`（20:05:33）→ **随后** Server `/quit` 重启
- TSF：`20:05:20 dll loaded 0.18.53`，`20:06:16 ActivateEx 0.18.53`
- Rime：`20:06:16` engine 启动，**20:06–20:07 无 `updated option: ascii_mode`**
- `20:07:37` 手动 redeploy（`langbar tray cmd id=40002`）→ `engine disposed` → **20:07:53 起** `ToggleOption ascii_mode` 正常
- `shift-guard`：`shift release eaten=1 tsf_combo=1`（Shift 单击仍被吞）
- `ToggleOption` 在 redeploy 后工作（0.18.53 Server 修复已验证）

### 根因（非配置/deploy 内容差异）

| 问题 | 根因 |
|------|------|
| 安装后热键无效 | **postinstall 顺序**：`reload-tsf` 在 Server `/quit` **之前**，TSF 连上即将重启的 Server，session 失效；首会话 Toggle 未达 Rime（日志 20:06–20:07 无 ascii 更新） |
| redeploy 后恢复 | 手动 redeploy 等价于 live TSF + maintenance 重建 engine/session |
| Shift 无效 | `ShouldEatShiftRelease()` / `g_shift_combo` 误标吞掉 Shift 释放 |
| 图标滞后 | `_UpdateLanguageBar` 的 `ShouldIgnoreAsciiSync()` 1.5s 防抖阻断 compartment 同步；`_ToggleAsciiMode` 未强制刷语言栏 |

配置层 `build/default.yaml`：Shift noop + Control+space **仍正确**。

### 0.18.54 方案

1. **PostInstall 顺序**：`deployquiet → purge-tray → quit → 重启 Server → reload-tsf → verify`（reload-tsf **必须在最终 Server 就绪后**）
2. **Shift guard**：`ShouldEatShiftRelease()` / 单击切换均用 `g_other_key_during_shift`（任意非 Shift 键按下即标记）；`OnShiftKeyUpAlone` 清除 `g_block_ascii_sync_until`
3. **语言栏**：`_ToggleAsciiMode` 后 `_UpdateLanguageBar(stat, force=true)` 绕过 ascii sync 防抖，立即写 compartment
4. **会话**：`ActivateEx` / `_ToggleAsciiMode` 在 Echo 失败时 `_Reconnect()` 刷新 IPC session
5. **保留** 0.18.52/0.18.53 已验证项：pending 仅 `*pfEaten`、System32 版本校验、`ToggleOption`+`_Respond`、`EnsureDeployAsciiConfig`

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| reload-tsf 在 Server quit 之前 | 0.18.52/0.18.53 日志证伪：安装后热键无效 |
| `ShouldEatShiftRelease` 含 `g_shift_combo` | 日志证伪：Shift 单击被吞 |
| 仅 `_ToggleAsciiMode` 更新 langbar 不刷 compartment | 图标滞后 1–2 键 |
| `_ToggleAsciiMode` 后 `_UpdateComposition` 拉 status | 0.18.53 审查：读到更旧 buffer |

---

## 0.18.54 验收（2026-06-13，用户反馈 FAIL）

### 结果

| 项 | 结果 |
|----|------|
| 安装 | `System32 version=0.18.54.0`；postinstall 顺序已修正（purge→quit→reload-tsf） |
| Shift / Ctrl+Space 切换 | **仍无效** |
| Shift 同时切换中英文标点 | **未实现**（用户要求 Shift 一次切换 ascii_mode + ascii_punct，非 Ctrl+Shift+3） |

### install.log / ascii.log 铁证（0.18.54，20:57–21:02）

- postinstall 顺序正确：`deploy` → `purge-tray` → `reload-tsf`（在 Server 重启**之后**）
- `weasel.tsf.log`：`WeaselTSF 0.18.54 ActivateEx`（20:58:50）
- **20:59–21:02 仅见** `shift-guard eaten=1 physical_keys=1`，**零条** `ToggleOption`
- `punct-guard block punct flip keycode=35`（用户误试 Ctrl+Shift+3，被 Rime ascii-guard 拦截——非用户需求的切换路径）

### 根因

| 问题 | 根因 |
|------|------|
| Shift 仍无效 | 0.18.54 `OnOtherKeyDuringShift()` 对**任意**非 Shift 键在 `GetKeyState(VK_SHIFT)` 为真时置位，**误把 Shift 单击标为组合键** → 释放时被吞，从未进入 `_ToggleAsciiMode` |
| 标点未随 Shift 切换 | 用户感知问题；Server `SetAsciiModeOnSession` **已** `ascii_punct=ascii_mode`，但 `_ToggleAsciiMode` 从未被调用故不生效 |
| Ctrl+Shift+3 | `weasel_hotkeys.yaml` 部署默认项，**非** 0.18.54 新增；用户要求的是 Shift 联动切换，不是单独快捷键 |

### 0.18.55 方案

1. **Shift guard 重写**：删除 `OnOtherKeyDuringShift`；仅用 `g_letter_key_seen`（A–Z/0–9）+ `g_non_shift_key_seen`（标点/IME 组合键）阻断切换；**Shift 单击** `ShouldBlockShiftToggle()==false` → `_ToggleAsciiMode`
2. **Shift 切换联动标点**：保留 `SetAsciiModeOnSession` 同时写 `ascii_mode` + `ascii_punct`（对齐 `ascii_composer.cc` L277）；日志标明 `ascii_mode+ascii_punct`
3. **Shift down 提前清状态**：`ProcessKeyEvent` 入口即 `OnShiftKeyDown()`，避免陈旧 guard
4. **诊断日志**：`weasel.tsf.log` 写 `shift release -> _ToggleAsciiMode` 便于验收
5. **保留** 0.18.52–0.18.54 已验证项：postinstall 顺序、pending 仅 `*pfEaten`、System32 版本校验、`ToggleOption`+`_Respond`、语言栏 force sync、`EnsureDeployAsciiConfig` Shift noop

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| `OnOtherKeyDuringShift` 对任意键 + `GetKeyState(VK_SHIFT)` | 0.18.54 日志证伪：physical_keys=1，零 ToggleOption |
| `g_shift_combo` 单独阻断 Shift 单击 | 0.18.52 日志证伪 |
| 让用户用 Ctrl+Shift+3 切换标点 | **非用户需求**；Shift 应联动 ascii_punct |
| 单独 `ToggleOption(ascii_punct)` 与 ascii_mode 分离 | 与 librime `ascii_composer` 行为不一致 |

---

## 0.18.55 验收（2026-06-13，用户反馈 FAIL）

### 结果

| 项 | 结果 |
|----|------|
| TSF 调用切换 | `shift release -> _ToggleAsciiMode` 出现（层 A 部分修好） |
| Server 执行切换 | **零条** `ToggleOption`（层 B 未修） |
| 用户体感 | 仍无效 |

### 根因（层 B，代码铁证）

`ClientImpl::ToggleAsciiMode()` 在 `_Active()` 为假时**静默 return**；`_Active()` 要求 `session_id != 0`。安装后 TSF 已连接 pipe 但 **未 StartSession** → `session_id=0` → 切换命令从未到达 Server。

### 0.18.56 方案（正面修层 B + 安装硬验收）

1. **`EnsureSession()`**：Connect 后强制 `StartSession` + `Echo` 校验；`ToggleAsciiMode`/`SetAsciiMode` 调用前必须 `EnsureSession()`
2. **TSF `_EnsureIpcSession()`**：`_ToggleAsciiMode` / `ActivateEx` / `_Reconnect` 统一走会话建立
3. **诊断日志**：session_id、Toggle 成功/失败写入 `weasel.tsf.log`
4. **PostInstall 硬验收**（任一失败则 postinstall 非 0）：
   - System32 FileVersion（14）
   - WeaselServer 进程运行（18）
   - IPC `EnsureSession` 探针（19）
   - `weasel.dll loaded VERSION` + `WeaselTSF VERSION ActivateEx`（15，45s 超时）
5. **install.nsi**：按退出码提示注销/重启/手动启服务，**禁止**未验收通过仍宣称成功

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅改 Shift guard 不改 IPC session | 0.18.55 日志：有 toggle 日志、无 ToggleOption |
| TSF 软等 tsf.log 不阻塞 success | 0.18.51–0.18.55 连续误报安装成功 |
| `_Active()` 要求 session 却不在切换前 StartSession | 根因本身，非修复方向 |

---

## 0.18.57 — 候选窗不显示（微信 inline 有预编辑）

### 用户验收（0.18.56）

| 项 | 结果 |
|----|------|
| Shift 切换中英文+标点 | **PASS**（`ToggleOption ascii_mode+ascii_punct`） |
| 微信输入 `ni'hao` 预编辑 | **PASS**（行内预编辑可见） |
| 候选字词面板 | **FAIL**（`weasel.ui.log` 无新 panel 记录） |
| 安装退出码 15 | System32+IPC 已通过；当前会话未热加载 TSF（真阴性） |

### 根因

微信等宿主在 `ITfUIElementMgr::BeginUIElement` 中将 `*pbShow` 置为 **FALSE**，而 0.18.56 的 `CCandidateList::StartUI` / `UpdateUI` 用 `_pbShow` 门控 `_MakeUIWindow()` 与 `Show()`。行内预编辑走 TSF Composition，不受 `_pbShow` 影响，故出现「有 `ni'hao`、无候选窗」。

次要：` _RefreshStatusFromResponse()` 在 Toggle 后可能读到陈旧 `ascii_mode`（日志 Server `1->0` 与 TSF `ascii=1` 不一致）。

### 0.18.57 方案

1. **候选窗**：`BeginUIElement` 的 `pbShow` 仅用于 TSF UIElement 同步；**始终** `_MakeUIWindow()`；有 `ctx.cinfo.candies` 时 `Show(TRUE)`，并写 `weasel.ui.log` `force show` 诊断
2. **Toggle 状态**：Toggle 后若 `ascii_mode` 未翻转则强制取反（防陈旧 buffer）
3. **安装**：`WaitForTsfReady` 合并 dll/ActivateEx 任一命中；reload 后 Sleep(3s)+60s 等待

### 实机验收门控（0.18.57）

- 微信中文模式打 `nihao`：`weasel.ui.log` 有 `panel force show` 或 `RBUTTONDOWN`；肉眼见候选条
- `weasel.ascii.log` 有 `ToggleOption`；`weasel.tsf.log` `ascii=` 与 Server 一致
- `install.log`：`verify tsf ready` 或 `WARN tsf not loaded`+退出码 15 提示明确

### 0.18.57 验收结果

| 项 | 结果 |
|----|------|
| 候选窗 | **FAIL**（`weasel.ui.log` 仍无 `force show` / `panel` 记录） |
| 0.18.57 已加载 | `weasel.dll loaded 0.18.57` 有，但修复未触及根因 |

### 证伪（0.18.57）

| 方案 | 结果 |
|------|------|
| 仅绕过 `_pbShow` 门控 | 日志无 `force show` ⇒ `ctx.cinfo.candies` 为空或 `UpdateUI` 未带候选；**非 pbShow 单因** |

---

## 0.18.58 — IPC 解析偏移 + 候选窗绘制顺序

### 根因（代码铁证 + 子代理交叉审查）

1. **`HandleResponseData` 从 `buffer.get()` 解析**，而 Server 将响应写在 `SendBuffer()`（`PipeMessage` 头之后）。`ctx.preedit` 行可能侥幸解析；**`ctx.cand` boost 归档行错位 → `candies` 恒空** ⇒ 无候选窗（微信/Cursor 均受影响）。
2. **次要**：`UpdateUI` 先 `Update`（`DoPaint`）后 `Show`，隐藏窗跳过绘制。
3. **安装卡死**：`Sleep(3s)+WaitForTsfReady(60s)` + 重复 `/upgrade`；退出码 15 触发 NSIS `Abort`。

### 0.18.58 方案

1. **`PipeChannel::HandleResponseData`**：改从 `SendBuffer()` + `_SendBufferSizeW()` 解析
2. **候选窗**：`Show(TRUE)` 在 `Update` 之前；无条件写 `weasel.ui.log` `panel candies=…`
3. **诊断**：Server `compose menu=N`；TSF `compose candies=N`
4. **安装**：去掉 postinstall 重复 upgrade；TSF 探测 8s；15=软成功（NSIS 不 Abort）

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅 `_pbShow` / `BeginUIElement` | 0.18.57 证伪 |
| 仅 Shift/IPC/session 修复 | 切换已 PASS，候选仍 FAIL |
| 安装 60s TSF 硬门控 + Abort on 15 | 卡死 + 误报安装失败 |
| postinstall 去掉 `/upgrade` 假定 NSIS 已更新 System32 | **0.18.58 exit 14**：System32=0.18.57，安装目录=0.18.58 |

### 0.18.58 验收结果

| 项 | 结果 |
|----|------|
| 安装 | **FAIL** exit 14：`skip upgrade` + `verify System32 ok=0` |
| 候选窗 | **未验收**（新 DLL 未进 System32） |

---

## 0.18.59 — 恢复 upgrade + 分层验收方案

### 根因（install.log 铁证）

```
postinstall: skip upgrade (NSIS already ran /upgrade)
System32 weasel.dll version=0.18.57.0 src_version=0.18.58.0
verify System32 ok=0
```

0.18.58 为缩短安装去掉 postinstall `/upgrade`，但 NSIS upgrade 未把 System32 升到 0.18.58，末尾校验失败。

### 0.18.59 方案

1. **postinstall 开头**：`RunUpgradeFromInstallDir` 最多 3 次，每次校验 FileVersion
2. **reload-tsf 后**：若 verify 失败再跑一轮 upgrade
3. **保留** 0.18.58 候选 IPC 修复（`SendBuffer` 解析、`Show` 在 `Update` 前）
4. **保留** TSF 8s 软检查、15=成功+警告

### 分层验收（最多 3 轮 × 3 方向 = 9 次检查可定案）

| 轮次 | 方向 | 检查项 | 通过标准 |
|------|------|--------|----------|
| **R1 安装层** | A System32 | `install.log` 末尾 | `verify System32 ok=1`，版本=安装包版本 |
| | B 服务 | 同上 | `verify WeaselServer running ok=1`，无 exit 18 |
| | C IPC | 同上 | `verify ipc session ok=1`，无 exit 19 |
| **R2 数据层** | D Server | `weasel.server.log` 打 `nihao` | `compose menu=N` 且 N>0 |
| | E TSF | `weasel.tsf.log` | `compose candies=N` 且 N>0 |
| | F UI | `weasel.ui.log` | `panel candies=N show=1` |
| **R3 体感层** | G 微信 | 中文打 `nihao` | 见候选条 |
| | H Cursor | 同上 | 见候选条 |
| | I Shift | 按 Shift | 中英文+标点同步切换 |

**分支定案**（每轮只修该层，不叠加已证伪方案）：

- R1 失败 exit 14 → System32/upgrade（本版修复点）
- R1 失败 exit 18/19 → 服务/IPC/注销重登
- R2：menu=0 → Rime/schema；menu>0 且 candies=0 → IPC 解析；candies>0 无 panel → UI 路径
- R3：R2 全过但不可见 → 定位/绘制/DPI

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| postinstall 跳过 upgrade | 0.18.58 exit 14 铁证 |

---

## 0.18.59 — 安装成功但输入中断

### 验收

| 项 | 结果 |
|----|------|
| 安装 | System32/Server/IPC 全过；exit 15 软警告 |
| 输入 | 用户报**完全无法输入** |

### 根因（日志 + 代码）

1. **install.nsi + `/reload-tsf` 强杀 `ctfmon.exe`**：Windows 输入法宿主被干掉，会话输入栈中断数分钟。
2. **强杀微信/钉钉等**：非官方行为；商业输入法不杀用户进程。
3. `verify tsf ready ok=0`（23:24:33）但 ActivateEx 在 23:27 才出现——8s 等待不足 + ctfmon 重启滞后。
4. `weasel.server.log` 有 `menu=5`，但 `weasel.tsf.log` **无 compose 行**、`weasel.ui.log` 无新 panel——TSF 未在用户应用内正常组字。
5. 存在 `Cursor.exe-weasel.dll*.dmp` 崩溃转储。

### 0.18.60 方案

1. **去掉** install.nsi 对微信/ctfmon/TextInputHost 的 taskkill
2. **`reload_tsf_gentle`**：`EnableLanguageProfile` 关→开，不杀 ctfmon
3. TSF 软检查等待 30s；提示改为「仅需重启已打开的应用」

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| taskkill ctfmon + start ctfmon | 0.18.59 输入中断铁证 |
| HandleResponseData 用 SendBuffer (+12) | **0.18.60 铁证**：Server menu>0 但 TSF 无 compose、UI 无 panel |

---

## 0.18.61 — IPC ReceiveBuffer 偏移 + 安装流程拆分

### 根因（0.18.60 日志铁证）

| 日志 | 现象 |
|------|------|
| `weasel.server.log` 23:57 | `compose menu=5` — Server 收到按键 |
| `weasel.tsf.log` | **无任何 `compose` 行** — TSF 未解析响应 |
| `weasel.ui.log` | 无新 `panel candies=` — UI 未更新 |

Pipe 大响应读入 client buffer 后，正文在 **offset sizeof(DWORD)=4**（`ReceiveBuffer`），非 `SendBuffer`(+12)。0.18.58 改 SendBuffer 仍错。

### 0.18.61 方案

1. **`HandleResponseData`** → `ReceiveBuffer()` + `_ReceiveBufferSizeW()`
2. **postinstall** 仅核心步骤，立即返回 0（不卡 30s）
3. **NSIS**：先弹「安装完成」→ 再 `/verify-install`（8s）→ 弹检查结果

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| HandleResponseData 用 buffer.get() | 偏移 0，错 |
| HandleResponseData 用 SendBuffer (+12) | 0.18.58–0.18.60 证伪 |
| HandleResponseData 用 ReceiveBuffer (+4) | **0.18.61 证伪**：Server menu>0，TSF 仍无 compose |
| ResponseParser `return bs.good()` 在 EOF | 成功解析后仍可能返回 false |

---

## 0.18.62 — action= 锚点扫描 + ResponseParser 修复

### 根因（0.18.61 日志，第三次尝试）

- Server `compose menu=5`（00:17–00:18）持续有
- TSF **仍无** `compose` / `edit-session` 行
- 结论：偏移猜测（SendBuffer/ReceiveBuffer）均不可靠；应扫描 `action=` 定位响应正文

### 0.18.62 方案

1. **HandleResponseData**：在 buffer 内搜索 `action=` 再解析
2. **ResponseParser**：遇到 `.` 结束返回 **true**（不修 EOF false）
3. **EditSession**：无条件写 `edit-session ok=… candies=…` 诊断行

### 0.18.62 验收

| 层 | 结果 |
|----|------|
| R1 安装 | 通过 |
| R2 数据 | **通过** — `menu=5`、`candies=5`、`panel show=1 hwnd=有效` |
| R3 体感 | **FAIL** — 候选字词窗肉眼不可见 |

### 根因（0.18.62 日志，第四次尝试定案）

R2 全过说明 IPC/TSF/Show 门控已通；问题在 **UI 分层窗绘制/定位**：

1. **`DoPaint` 1210–1214**：`!IsWindowVisible()` 早退跳过 `_LayerUpdate` → `WS_EX_LAYERED` 窗无像素内容，仅 `show=1` 仍透明
2. **`Refresh` 未用 caret 回退**：`MoveTo` 有 `GetCaretOrCursorRect`，但 `Refresh` 直接 `_RepositionWindow`，首帧 `m_inputPos={0,0,0,0}` 定位错误
3. **`m_ctx==m_octx` 跳过 RedrawWindow**：组字中候选不变时可能不重绘
4. **Show 在 Update 之前**：分层窗应先 `UpdateLayeredWindow` 再 `Show`

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 再改 IPC 偏移 / action= 锚点 | 0.18.62 R2 已通过 |
| 仅调 `_pbShow` / Show 顺序（0.18.57） | 不足，R3 仍 FAIL |

---

## 0.18.63 — 分层窗 LayerUpdate + 定位回退

### 0.18.63 方案

1. **`DoPaint`**：去掉 `!IsWindowVisible()` 早退，**始终** `_LayerUpdate`；写 `paint drawn=… visible=… @x,y` 诊断
2. **`Refresh`**：`m_inputPos` 无效时用 `GetCaretOrCursorRect`；组字有候选时强制 `RedrawWindow`
3. **`UIImpl::Show`**：从隐藏恢复时补 `RedrawWindow`
4. **`UpdateUI`**：先 `Update`（绘制）再 `Show`，最后 `Refresh`

### 验收标准

- 记事本中文打 `nihao`：肉眼见候选条
- `weasel.ui.log`：`panel candies=5 show=1` + `paint drawn=1 … @非0坐标`

### 0.18.63 验收

| 层 | 结果 |
|----|------|
| R2 数据 | 通过 — `panel candies=5 show=1 hwnd=有效` |
| R3 体感 | **FAIL** — 仍不可见 |
| 关键证据 | `weasel.ui.log` **无** `paint` 行 → `DoPaint` 未执行或零尺寸早退 |

### 根因（0.18.63 日志，第五次定案）

- `ResponseParser` 写入 `_cand->style()`（`_style` 成员）
- `WeaselPanel` 读取 `_ui->style()`（`m_style` 引用）
- `UpdateUI` **未同步** 两者 → 面板用默认 `font_point=0`、透明色 → `GetContentSize()` 为 0 → 候选窗零尺寸

### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅修 LayerUpdate / IsWindowVisible / caret 回退 | 0.18.63 无 `paint` 行，尺寸仍为 0 |

---

## 0.18.64 — 同步 _ui->style() 与 _style

### 0.18.64 方案

1. **`UpdateUI`**：每次更新前 `_ui->style() = _style`（服务端样式进面板）
2. **诊断**：`refresh size=… font=…`；零尺寸时 `paint skip zero-size font=…`

### 验收标准

- 记事本 `nihao` 见候选条
- `weasel.ui.log`：`refresh size=宽x高 font=14` + `paint drawn=1`

### 0.18.64 验收

| 层 | 结果 |
|----|------|
| R2 | 通过 — `panel candies=5 show=1` |
| R3 | **FAIL** — 仍不可见 |
| 关键证据 | 全程 **无** `refresh`/`paint` 行；`font` 未记录 |

### 根因（第六次定案，代码级）

1. **`AddSession` → `_Respond`** 写入 `style` 并设 `__synced=true`
2. **TSF `StartSession`** 只读 session id（DWORD），**不解析** pipe 正文
3. 下一帧 `ProcessKeyEvent` 覆盖 buffer → **style 丢失**
4. 首键响应因 `__synced=true` **不再带 style** → `_style.font_point=0` → 窗体 0×0
5. 0.18.64 的 `_ui->style()=_style` 同步无效，因 `_style` 本身从未被填充

### 证伪

| 方案 | 结果 |
|------|------|
| 仅同步 `_ui->style()=_style` | `_style` 仍 font=0 |
| 仅修 LayerUpdate/定位 | 无 `refresh` 行，尺寸为 0 |

---

## 0.18.65 — AddSession 后重置 __synced + 样式诊断

### 0.18.65 方案

1. **`AddSession`**：`_Respond` 后 `session_status.__synced = false`，强制首键响应带 `style`
2. **保留** `UpdateUI` 中 `_ui->style() = _style`
3. **诊断**：`panel … font=`、`refresh enter font=`、`refresh size=`

### 验收标准

- 记事本 `nihao` 见候选条
- `weasel.ui.log`：`font=14` + `refresh size=非0` + `paint drawn=1`
- **安装后须重启正在输入的应用**（加载新 weasel.dll）

### 0.18.65 验收

| 层 | 结果 |
|----|------|
| R1 安装 | 通过 |
| R2 数据 | 通过 |
| R3 体感 | **通过** — 用户确认候选字词窗可见、可输入 |

---

## 总复盘：候选窗不可见（0.18.56–0.18.65）

### 问题表象

- 可组字、可输入，但**候选字词窗肉眼不可见**
- 日志常同时出现：`menu>0`、`candies>0`、`panel show=1 hwnd=有效`

### 分层定案法（可复用）

| 轮次 | 查什么 | 日志关键字 | 通过标准 |
|------|--------|------------|----------|
| R1 安装 | System32 / Server / IPC | `install.log` | DLL 版本一致、服务在跑 |
| R2 数据 | Server → TSF → UI 链路 | `menu=`、`candies=`、`panel show=` | 三者均 >0 |
| R3 体感 | 肉眼 | 记事本打 `nihao` | 见候选条 |

**分支规则**：R2 全过但 R3 失败 → 只查 UI（绘制/样式/定位），**不要再动 IPC**。

### 六次尝试收敛路径

| 版本 | 假设 | 结果 | 结论 |
|------|------|------|------|
| 0.18.58–0.61 | IPC buffer 偏移 | TSF 无 compose | 偏移猜测不可靠 |
| 0.18.62 | `action=` 锚点扫描 | **输入恢复** | IPC 已通 |
| 0.18.63 | LayerUpdate / 定位 | 仍不可见 | 无 `paint` 行 |
| 0.18.64 | `_ui->style()=_style` | 仍不可见 | `_style` 本身 font=0 |
| **0.18.65** | `AddSession` 后 `__synced=false` | **R3 通过** | **根因：style 从未到达 TSF** |

### 最终根因（一条链）

```
AddSession._Respond 写入 style 并 __synced=true
  → TSF StartSession 只读 session id，不解析 pipe 正文
  → 下一键 ProcessKeyEvent 覆盖 buffer，style 丢失
  → 首键响应因 __synced=true 不再带 style
  → _style.font_point=0 → 面板布局 0×0 → 候选窗透明/不可见
```

### 有效修复（0.18.65，三处配合）

1. **`RimeWithWeasel.cpp` `AddSession`**：`_Respond` 后 `session_status.__synced = false`，强制首键 IPC 带 `style`
2. **`CandidateList::UpdateUI`**：`_ui->style() = _style`（面板读 `_ui->style()`，解析写 `_style`）
3. **UI 层（0.18.63）**：去掉 `DoPaint` 的 `IsWindowVisible` 早退、caret 回退、组字时强制 `RedrawWindow`（style 到位后才生效）

### 诊断日志判读

| 日志 | 含义 |
|------|------|
| `panel candies=N show=1` 无 `refresh`/`paint` | 旧 DLL 未重载，或 `font=0` 未进 Refresh |
| `font=0` | style 未从 Server 下发到 TSF |
| `font=14` + `refresh size=0x0` | 样式有了，布局/D2D 异常 |
| `paint drawn=1 @x,y` | 绘制链已通，再查定位/DPI |

### 证伪清单（勿再试）

- IPC 固定偏移（SendBuffer/ReceiveBuffer）
- 仅调 Show 顺序 / `_pbShow` 门控
- 仅 LayerUpdate 不修 style 同步
- 仅 `_ui->style()=_style` 不修 `__synced`
- 安装时 `taskkill ctfmon` / 强杀微信
- postinstall 跳过 `/upgrade`（exit 14）

### 验收注意

- 安装新版本后须**重启已打开的应用**（非整机重启），否则仍加载旧 `weasel.dll`
- `verify tsf ready` 仅作手动诊断；**不应阻塞安装向导**

---

## 0.18.66 — 恢复简洁安装体验

### 背景

调试期加入 `/verify-install` 分步弹窗、postinstall 硬验收（exit 18/19），拖慢安装且易误报。

### 0.18.66 方案

1. **NSIS**：去掉 `/verify-install` 及「接下来将自动检查…」；postinstall 成功即弹「安装完成」
2. **postinstall**：保留 upgrade → deploy → reload-tsf；System32/Server/IPC **仅写日志，不阻塞**
3. **`/verify-install`**：保留为手动诊断命令，安装程序不再调用

### 安装流程（用户可见）

```
/upgrade → /postinstall → 「安装完成」
```

失败仅在实际步骤失败时提示（deploy、reload-tsf、upgrade 等）。

### 0.18.67 — 去掉 InstFiles 阶段多余 MessageBox

- 移除 postinstall 成功后的 `MessageBox`（与进度窗叠层）；完成提示仅保留 NSIS 标准「安装完成」页（`MUI_PAGE_FINISH`）。

---

## 0.18.68 — Cursor 闪退排查 + Shift 选候选互斥

### 问题 A：Cursor 闪退是否与输入法有关

#### 证据（2026-06-14 上午）

| 时间 | 证据 | 含义 |
|------|------|------|
| 08:40:59 | `Cursor.exe-weasel.dll-20260614-084059.1476.dmp` | weasel UEF 捕获未处理异常 |
| 08:47:34 | `Cursor.exe-weasel.dll-20260614-084734.7920.dmp` | 同上，任务执行中再次崩溃 |
| 08:48:57 | `Cursor.exe-weasel.dll-20260614-084857.17232.dmp` | 同上，第三次 |
| 08:48:39 | `weasel.tsf.log` 末行：正常 `edit-session ok=1` | 崩溃前无 TSF C++ 异常日志 |
| 08:48:58 | `startup weasel.dll loaded 0.18.67` | Cursor 重启后重载 TSF |

**结论（确定）**：闪退与 **weasel.dll 在 Cursor 进程内的原生崩溃** 强相关（UEF 已写转储）。  
**未确定**：具体函数/模块（需 WinDbg 分析转储栈）。

#### 行动框架（多方向并行，互不保证一次修完）

| 方向 | 假设 | 局部改动 | 验收 | 证伪 |
|------|------|----------|------|------|
| **D1 诊断** | 缺异常码无法定栈 | UEF 写 `weasel.tsf.log` `[crash] code=… addr=…` | 下次崩溃日志有 code | 仍无 code → UEF 未触发 |
| **D2 UI 线程** | 分层窗绘制/定位 | `WeaselPanel` 空指针/跨线程 HWND 防护 | Cursor 打长句不再崩 | 有 code 但栈不在 UI |
| **D3 EditSession** | composition 重入 | `DoEditSession` 边界加固 | 高频输入不崩 | 栈在 TSF 非 EditSession |
| **D4 IPC** | pipe 重入/竞态 | 已有 guard 复核 | server 无异常 | 栈在 librime 非 IPC |

0.18.68 **仅实施 D1**（最小风险）；D2–D4 待转储 `code/addr` 或 WinDbg 栈后再动。

### 问题 B：Shift 选第 2/3 候选时同时切换中英文

#### 根因（代码审查）

`KeyEventSink.cpp` 在 `_ShouldAllowShiftCandidatePick==true` 时虽跳过 TSF 层 `_ToggleAsciiMode`，但仍将 `Shift_L/R` **送入 Rime `ProcessKeyEvent`**。Rime 侧 `key_binder`（选候选）与 `ascii_composer`（Shift 切模式）**同键双绑**，导致选词与切模式同时发生。

#### 方案辩论（制定 vs 审查）

| 方案 | 改动 | 优点 | 风险 |
|------|------|------|------|
| **B1（采用）** | Shift 候选成立时 `_SelectCandidateOnCurrentPage(1/2)` 并 `return`，不送 Shift 给 Rime | 改动 10 行内；不动 Rime/安装 | 硬编码 index 1/2（与默认 Shift_L/R 一致） |
| B2 | Rime 方案改键位互斥 | 彻底 | 动用户配置，范围大 |
| B3 | 仅加强 `_ShouldAllowShiftCandidatePick` 条件 | 更小 | 不解决 Rime 双绑，证伪 |

**审查结论**：B1 最优——局部、可日志验收（`[candidate] shift-pick index=…`），不触发新安装问题。

#### 0.18.68 改动

1. `KeyEventSink.cpp`：Shift 松开且候选够数 → IPC 选候选 → 早退
2. `dllmain.cpp`：UEF 写 `[crash]` 诊断行

#### 验收门控（用户实测）

| 项 | 通过标准 |
|----|----------|
| Shift 选候选 | 组字 ≥2/3 候选时按左/右 Shift：`weasel.tsf.log` 有 `shift-pick index=1/2`，**无**紧随的 `toggle shift release` |
| 单击 Shift 切模式 | 非组字时 Shift 松开仍 `toggle` 正常 |
| Cursor 稳定性 | 无法保证一次解决；若再崩，查 `[crash] code=` 行 |

#### 证伪（勿再试）

| 方案 | 结果 |
|------|------|
| 仅 TSF 跳过 `_ToggleAsciiMode` 仍 `ProcessKeyEvent(Shift)` | 根因本身，0.18.68 前状态 |
| 未验 Shift 候选即打包 | 禁止 |

### 0.18.68 验收结果（用户反馈 FAIL）

| 项 | 结果 |
|----|------|
| `shift-pick` 日志 | **零条** — B1 门控从未命中 |
| Shift 选候选 + 切模式 | **仍 FAIL** |
| Cursor 闪退 | **仍 FAIL**（09:49 后无新 UEF dmp，可能未重启 Cursor 或崩溃形态不同） |

**0.18.68 证伪**：仅当 `_ShouldAllowShiftCandidatePick==true` 才 IPC 选词；实测该条件恒 false（`_status.composing` 与 `_cand->GetCount` 在 Shift 松开时不同步），修复未生效。

### 0.18.69 — Shift 门控放宽 + 有候选时禁止 toggle

#### 改动

1. `_ShouldAllowShiftCandidatePick`：`composing \|\| cand_count>=2` 即可左 Shift；右 Shift `cc>=3` 或 `composing && cc>=2`
2. `_HasVisibleCandidateMenu`：有候选菜单时 **禁止** `_ToggleAsciiMode`（`shift-skip-toggle`）
3. 每次 Shift 松开写 `shift-diag`（composing/cc/guard 状态）
4. `WeaselAppendLogW` 增加 `FlushFileBuffers`，便于 crash 行落盘

#### 排查预算（最多几轮可穷尽）

| 问题域 | 分支数 | 每分支尝试上限 | 合计 |
|--------|--------|----------------|------|
| Shift 选词/切模式 | 3（TSF 门控 / 有候选禁 toggle / Rime 键位） | 各 1–2 轮 | **≤6 次** |
| Cursor 闪退 | 4（D1 诊断 / D2 UI / D3 EditSession / D4 IPC） | 各 1–2 轮 | **≤8 次** |
| **总计** | | 交叉验证 3 轮 | **≤9–12 次实机迭代** |

超过 12 次仍 FAIL 则需 WinDbg 符号栈或用户侧 ProcDump，属工具链瓶颈而非代码盲试。

#### 验收门控（0.18.69）

- 组字按左/右 Shift：`shift-diag allow=1` → `shift-pick index=1/2`；**无** `toggle shift release`
- 有候选但条件不足：`shift-skip-toggle candidates visible`；不切模式
- 无候选单击 Shift：仍有 `toggle shift release`
- Cursor 再崩：`weasel.tsf.log` 有 `[crash] code=0x…`

### 0.18.69 验收结果（用户反馈 FAIL）

| 项 | 证据 |
|----|------|
| Shift | `shift-diag composing=1 cc=0 combo=1 block=1 allow=0` — 门控仍失败 |
| 飞书崩溃 | `Feishu.exe-weasel.dll-20260614-102301.dmp` |
| Cursor 崩溃 | `Cursor.exe-weasel.dll-20260614-102409.dmp` |
| 异常码 | `[crash] code=0xE06D7363`（MSVC C++ 异常） |

**0.18.69 证伪**：依赖 `GetCount`/`combo` 门控；组字时 `cc=0` 且 `combo=1` 恒阻止选词；Shift 仍送 Rime 或走 toggle。

### 0.18.70 — 组字期 Shift 隔离 + 宿主崩溃防护

#### Shift（B3）

组字中（`composing && !ascii`）：
- Shift **不下发** Rime、**不** `_ToggleAsciiMode`
- 单击左/右 Shift → IPC `shift-pick index=1/2`
- 仅 `g_letter_key_seen`（真 Shift 组合键）时吞键

#### 崩溃（D2 局部）

1. `TryDeserialize`：`catch(...)`，去掉 IPC 异常 `MessageBoxA`（宿主进程内弹窗可致崩溃）
2. `GetResponseData`：`catch(...)`
3. `_ProcessKeyEvent`：外层 `catch(...)` 写 `key-event uncaught exception`

#### 验收

- 飞书/记事本组字 + 左/右 Shift：`shift-pick index=1/2 composing=1`，无 `toggle`
- 非组字 Shift：仍 `toggle shift release`
- 崩溃：不应再出现 `0xE06D7363` 转储（或日志有 `key-event uncaught exception` 而非进程退出）

### 0.18.70 验收结果（用户反馈 PASS — 快捷键）

| 项 | 结果 |
|----|------|
| **Shift 选第 2/3 候选** | **PASS** — 组字期左/右 Shift 选词正常，不再与中英文切换冲突 |
| 非组字 Shift toggle | 待用户补充确认（用户仅明确快捷键已修复） |
| 飞书/Cursor 崩溃 | **FAIL**（见 C-D2 证伪；10:38/10:42 Cursor dmp） |

**0.18.70 证真（Shift B3）**：组字期完全隔离 Rime/toggle、改走 IPC `shift-pick`，绕过 `cc=0`/`combo` 门控失效链。0.18.69 门控路线证伪，勿再依赖 `GetCount`+`ShiftComboGuard` 组合放行选词。

### 0.18.71 — EditSession 重入隔离 + 全链路异常防护（C-D3）

#### 日志铁证（2026-06-14 用户反馈）

| 宿主 | 转储 | 异常 |
|------|------|------|
| Cursor ×2 | `Cursor.exe-weasel.dll-20260614-103805.dmp`、`104208.dmp` | `0xE06D7363` @ `00007FFB84EA1B6A` |
| 钉钉 ×1 | 未在 `%TEMP%\rime.weasel\` 发现 weasel 转储 | 用户报告闪退，疑同 TSF 类 |

`weasel.tsf.log`：崩溃时刻有 `[crash]` 行；**无** `unknown exception in DoEditSession` → 异常在 DoEditSession 外层或未捕获重入路径。

#### 改动（C-D3）

1. `DoEditSession`：`_edit_session_depth>0` 时禁止 sync 重入，仅标 `_composition_dirty`
2. 移除 DoEditSession 末尾 sync `_UpdateComposition`；改 `_FlushDeferredCompositionActions` 异步刷新
3. `_UpdateUI` / `CCandidateList::UpdateUI` / `ContextUpdater::_StoreCand` / `Styler::Store` / `ResponseParser::Feed` try/catch
4. `CGetTextExtentEditSession` / `CInlinePreeditEditSession`：`RunEditSessionSafe`

#### 验收门控

- 安装 0.18.71 后**重启** Cursor、钉钉、飞书
- 各应用中文输入 10 分钟无新 `*weasel.dll*.dmp`
- `weasel.tsf.log` 无新 `[crash] code=0xE06D7363`

### 0.18.71 验收结果（用户反馈 FAIL）

| 项 | 结果 |
|----|------|
| 安装后开 Cursor | **FAIL** — 立即闪退 |
| 数分钟内 Cursor | **FAIL** ×2（第二次**非输入状态**） |
| 微信 | **FAIL** — 非输入时闪退 |
| 任务管理器结束 WeaselServer | **FAIL** — 退出过程中 Cursor 第三次闪退 |

| 转储 | 时间 |
|------|------|
| `Cursor.exe-weasel.dll-20260614-111115.dmp` | 11:11:15 |
| `Cursor.exe-weasel.dll-20260614-111331.dmp` | 11:13:31 |
| `Cursor.exe-weasel.dll-20260614-111451.dmp` | 11:14:51 |

日志：`[crash] code=0xE06D7363 addr=00007FFB84EA1B6A`（与 0.18.70 相同）；11:15:02 后大量 `Reconnect Connect failed`（用户结束服务）。

**C-D3 证伪**：非输入态仍崩；杀服务后 IPC 断连与崩溃时间吻合 → 下一跳 C-D4（Deactivate/诊断日志）。

### 0.18.72 — 崩溃专档 + 退出路径加固（C-D4）

#### 改动

1. `WeaselCrashDiag.h`：`weasel.crash.log`（`report` 摘要 + 40 条 `crumb`）；VEH 记录 `0xE06D7363`
2. `Deactivate`：`Disconnect` 替代裸 `EndSession`；全程 try/catch + crumb
3. `ClientImpl::EndSession` / `Disconnect`：IPC 异常吞掉
4. `CRASH_DIAGNOSTICS.md`：按 crumb 定分支，禁止盲目试错

#### 验收

- 若再崩：`weasel.crash.log` 必须有 `report` 与崩溃前 `crumb` 链
- 结束 WeaselServer 后 Cursor 不应闪退

### 0.18.72 验收结果（用户反馈 FAIL）

| 项 | 证据 |
|----|------|
| 微信闪退 | `12:11:34` `host=Weixin.exe ver=0.18.72` `depth=1 composing=0 ipc=1` |
| Cursor 闪退 | 用户反馈再次出现 |
| crumb 铁证 | 崩溃前 `edit-session begin/end ok` **数十次/秒**（12:11:21–23） |

**C-D4 证伪**：诊断机制有效（有 report+crumb），但仅记录未防崩；根因为 **EditSession 空转风暴**（无用户按键仍反复 GetResponseData）。

### 0.18.73 — EditSession IPC 门控 + 节流（C-D5）

#### 改动

1. `_RequestIpcRefresh()`：仅按键/选词/删词后允许 IPC 拉取
2. `DoEditSession`：无刷新请求则 `skipped noop`；200ms 内 >30 次则 `throttled`
3. `_FlushDeferredCompositionActions`：无 `_ipc_refresh_requested` 时丢弃 `composition_dirty`，打断自激环

#### 验收

- 微信/Cursor 正常使用 10 分钟无新 dmp
- `weasel.crash.log` 崩溃前不应再出现连续 `begin/end ok` 刷屏

### 0.18.73 验收结果（用户反馈 FAIL）

| 项 | 证据 |
|----|------|
| 宿主闪退 | 仍未修复（飞书/Cursor/微信等） |
| Shift 复发 | 组字时左/右 Shift 选 2/3 字又错误切英文 |
| 根因 | C-D5 门控/节流使 `_status.composing` 滞后；组字期 Shift 误判走 `_ToggleAsciiMode`；`_FlushDeferredCompositionActions` 丢弃 `composition_dirty` |

**C-D5 证伪**：风暴缓解思路对，但实现误伤用户显式操作与 Shift 状态机。

### 0.18.74 — Shift 状态修复 + urgent IPC（S-B4 / C-D5b）

#### 改动

1. `_IsActivelyComposing()`：`_status.composing` + TSF `_pComposition` + 候选 UI，不依赖滞后 IPC 状态
2. `_RequestIpcRefresh(urgent)`：按键/选词/删词标 urgent，豁免 EditSession 节流与 noop 跳过
3. `_SelectCandidateOnCurrentPage`：shift-pick 后 **同步** `_UpdateComposition(ctx, TRUE)`
4. `_FlushDeferredCompositionActions`：不再静默丢弃 `composition_dirty`

#### 验收

- 组字打 `nihao`，左 Shift 选第 2 字、右 Shift 选第 3 字，不切英文
- 微信/Cursor 10 分钟无新 dmp
- `weasel.crash.log` 无连续 `begin/end ok` 刷屏

### 0.18.74 验收结果（用户反馈 FAIL — 切换；崩溃暂未复现）

| 项 | 证据 |
|----|------|
| Shift 选词 | 未反馈复发（相对 0.18.73 可能改善） |
| 中英文/标点切换 | **失效**：空闲单击 Shift 无法切换 |
| 闪退 | 暂未复现 |

**S-B4 证伪（切换子项）**：`_IsActivelyComposing()` 过宽（`_pComposition` 残留 + `cand>=1`），空闲 Shift 被组字隔离分支吞掉，永远进不了 `_ToggleAsciiMode`。

### 0.18.75 — 收窄组字 Shift 隔离（S-B5）

#### 改动

1. `_ShouldIsolateShiftForComposing()`：仅 `_status.composing` 或 **候选>=2**（可见菜单）时隔离
2. Shift 抬起切换：去掉 `!_IsActivelyComposing()` 门控（组字分支已 early-return）
3. **保留** urgent IPC、shift-pick 同步刷新（0.18.74 有效部分）

#### 验收

- 空闲：Shift 单击切换中英文与标点；日志应有 `shift release -> _ToggleAsciiMode`
- 组字：左/右 Shift 选 2/3 字不切英文；日志应有 `shift-pick index=1/2`
- Ctrl+Space 仍可切换

### 0.18.75 验收结果（用户反馈 PARTIAL）

| 项 | 结果 |
|----|------|
| 组字 Shift 选 2/3 字 | **PASS** |
| 空闲 Shift 切换 | 托盘可切英文，但**下一键弹回中文**，输入仍为中文 |
| 闪退 | 暂未复现 |

**根因**：`_ToggleAsciiMode` 在 IPC 未确认时**乐观翻转**本地 `ascii_mode` 并强刷托盘；`_ascii_toggle_tick_=0` 使 EditSession 下一键 `GetResponseData` 用服务端真实状态（中文）覆盖；托盘弹回但 Rime 从未真正切英文。

### 0.18.76 — Toggle 同步修复（T-B1）

#### 改动

1. 切换前 `_RefreshStatusFromResponse` 取真实 `ascii_before`
2. **删除**乐观本地取反；仅以 IPC 确认 `ascii_mode==target` 为成功
3. 未同步时 `SetAsciiMode(target)` 兜底再读
4. 成功后 `_ascii_toggle_tick_=GetTickCount()`，配合 EditSession 1.5s 防抖

#### 验收

- 空闲 Shift → 托盘英文 → 继续输入字母为**英文直出**（非拼音候选）
- 再 Shift → 回中文，标点随 `full_shape` 同步
- 组字 shift-pick 仍 PASS

### 0.18.76 验收结果（用户反馈 FAIL）

| 项 | 结果 |
|----|------|
| 组字 shift-pick | **PASS** |
| 空闲 Shift | **无反应**，托盘不切换 |
| 重启算法服务后 | Cursor **闪退一次** |

**日志铁证**（13:26:12 起）：
- `ToggleAsciiMode sync failed; skip langbar update` 连续出现
- `weasel.ascii.log` 仅有 `SetOption ascii_mode=1`（T-B1 兜底），无配套 UI 更新
- Cursor 崩溃：`13:25:04 host=Cursor.exe depth=1 ipc=1`，崩溃前 `edit-session begin ipc` 刷屏

**T-B1 证伪**：严格 sync 门控 + `SetAsciiMode` 二次写入，在 pipe 状态滞后时完全阻断切换。

### 0.18.77 — Toggle 恢复 + Reconnect 加固（T-B2）

#### 改动

1. `ToggleAsciiMode` IPC 成功后：刷新成功用服务端状态，否则取反 `ascii_before` 并刷托盘（**不再** `sync failed` 静默返回）
2. **移除** `SetAsciiMode` 兜底（避免二次切换）
3. `_Reconnect`：重置 EditSession 标志 + Disconnect/GetResponseData try/catch

#### 验收

- 服务重启后：空闲 Shift 托盘切换；日志 `ToggleAsciiMode ok ascii=0/1` 交替
- 组字 shift-pick 仍 PASS
- 重启 WeaselServer 后 Cursor 10 分钟无新 dmp

### 0.18.77 验收结果（用户反馈 FAIL — 切换回弹）

| 项 | 结果 |
|----|------|
| 组字 shift-pick | **PASS** |
| 空闲 Shift | 托盘可切换，**下一键回中文** |
| 日志 | `ToggleAsciiMode ok ascii=1` **三次均为 1**（无 0/1 交替）；切换后立刻 `composing=1` 拼音候选 |

**T-B2 证伪**：切换前 `_RefreshStatusFromResponse` 读**上一次按键**陈旧 buffer，`ascii_before` 恒为 0 → 每次目标都是英文；托盘仅本地/Compartment 变化，Rime 仍为中文。

### 0.18.78 — SetAsciiMode 显式切换（T-B3）

#### 改动

1. 用 `SetAsciiMode(target)` 替代 `ToggleAsciiMode`；**删除**切换前 refresh
2. `ascii_before` 取自本地 `_status`（含 tick 保护）
3. Server `ENABLE/DISABLE_ASCII` 后 `_Respond` 写 pipe 状态
4. EditSession tick 同步 `full_shape`；tick 窗口内 force 刷托盘

#### 验收

- Shift → 英文 → 输入 `hello` **无拼音候选**、托盘保持 EN
- 再 Shift → 中文 + 中文标点
- 组字 shift-pick 仍 PASS

### 0.18.78 验收结果（用户反馈 — 快捷键 PASS / 崩溃 FAIL）

| 项 | 结果 |
|----|------|
| Shift 切换 + 组字 shift-pick | **PASS**（用户确认） |
| 微信/Cursor 闪退 | **FAIL** `13:52:20` Cursor、`13:52:22` 微信；`depth=1 ipc=1` |
| crumb | 崩溃前仍见 `edit-session begin ipc` 密集刷屏 |

**T-B3 证真（快捷键）**；崩溃仍为 EditSession IPC 风暴。

### 0.18.79 — EditSession 风暴抑制（C-D6）

#### 改动（**不修改** `_ToggleAsciiMode` / shift 隔离）

1. 去掉 `_ProcessKeyEvent` 冗余 refresh；sync `_UpdateComposition` 统一拉 IPC
2. 200ms 内 >25 次全局限流；节流时保留 refresh 意图
3. `_ipc_refresh_force`：shift-pick / 删词豁免限流
4. `GetResponseData` 异常吞掉，防 `0xE06D7363` 穿透宿主
5. tick 窗口仅纠正 ascii 时 force 刷托盘

#### 验收

- 快捷键与组字 shift-pick 仍 PASS
- 微信/Cursor 30 分钟无新 dmp

**FAIL @ 0.18.79**：微信输入约 1 分钟崩溃 `14:07:45`；`host=Weixin.exe ver=0.18.79 depth=1 composing=0 ipc=1`；`14:07:37–38` 同秒多次 `begin ipc`；`14:07:30–31` 大量 `composing=0 preedit=0 candies=0` 空转；日志零条 `throttled`。

**C-D6 证伪**：限流未触达（风暴在 eaten 路径前已排队）；根因是未吞键也 sync `_UpdateComposition` 拉 IPC。

### 0.18.80 — 源头减风暴（C-D7）

#### 改动（**不修改** `_ToggleAsciiMode` / S-B5 shift 隔离 / T-B3 Server Respond）

1. `KeyEventSink`：`ShouldPullIpcAfterRimeKey` — 仅 `*pfEaten || _status.composing` 时 sync `_UpdateComposition`（四处 OnKey/TestKey）
2. `EditSession`：18/200ms 全局限流；非 force 时 12ms 最小 `begin ipc` 间隔（`paced` crumb）
3. 保留 C-D6：`force` 豁免 shift-pick；IPC 异常吞掉；tick 保 full_shape

#### 验收

- 快捷键与组字 shift-pick 仍 PASS
- 微信/Cursor 30 分钟无新 dmp
- 日志应见 `throttled`/`paced`；`composing=0` 空转明显减少

**FAIL @ 0.18.80**（用户验收）：
1. 微信仍崩溃 `14:18:07/15` ver=0.18.80
2. Cursor 候选窗不可见；微信可见
3. 快捷键 PASS
4. `weasel.ui.log`：`font=0 size=0x0 paint skip zero-size` — C-D7 跳过 GetResponseData 致 pipe 内 style 被后续键覆盖（0.18.65 同类根因）

**C-D7 证伪**：不能用 eaten/composing 门控跳过 IPC 拉取。

### 0.18.81 — 每键必拉 IPC + 空闲减负（C-D8）

#### 改动（**不修改** `_ToggleAsciiMode` / S-B5 / T-B3）

1. **撤销 C-D7**：`_rime_key_dispatched` 后一律 sync `_UpdateComposition`（WeaselIME 模式，防 style 覆盖）
2. **EditSession**：仅 `_status.composing==0` 时 8/200ms 节流与 12ms paced；节流时 `DrainIdleIpcResponse` 仍消费 pipe
3. **idle-skip**：已加载 style 且空响应时跳过 TSF/UI 重操作
4. **布局**：`_FlushDeferredCompositionActions` 在 `_status.composing` 时也触发 `_UpdateCompositionWindow`

#### 验收

- Cursor / 微信候选窗可见（`font>0`、`refresh size=非0`）
- 快捷键与组字 shift-pick 仍 PASS
- 微信/Cursor 30 分钟无新 dmp

**FAIL @ 0.18.81**（用户验收）：微信/Cursor **快速**崩溃。
- 微信 `14:27:17` `0xE06D7363` depth=0 ActivateEx
- Cursor `14:27:37` `0xC0000005` depth=1；同秒大量 `begin ipc` + `idle-skip`
- **C-D8 证伪**：每键 sync EditSession 风暴未消除；paced/drain 双读 pipe 仍崩

### 0.18.82 — 按键线程 pull + 空闲不进 ES（C-D9）

#### 改动（**不修改** `_ToggleAsciiMode` / S-B5 / T-B3）

1. `_PullResponseAfterKey`：按键回调内立即消费 pipe（防 style 覆盖，0.18.65 铁律）
2. `_AfterRimeKey`：空闲响应仅更新 langbar，**不**进 EditSession
3. 组字/提交：用已 pull 缓存进 ES；持续组字默认 **async** 合并（`_pending_key_pull`）
4. 保留 C-D8 布局修复；移除 C-D8 节流/drain/idle-skip

#### 验收

- Cursor / 微信候选窗可见
- 快捷键与组字 shift-pick 仍 PASS
- 微信/Cursor 30 分钟无新 dmp
- 日志：空闲键 `key-ipc idle ok`；组字 `key-ipc es async/coalesce`；无秒级 `begin ipc` 刷屏

**FAIL @ 0.18.82**（用户验收）：第二字母卡死；候选窗无法消失；微信崩溃。
- 铁证：`key-ipc es async` 后连续 `edit-session skipped noop`
- **根因**：async `_UpdateComposition` 未设 `_ipc_refresh_requested`；空闲路径跳过 EndUI/HideUI
- **C-D9 证伪**

### 0.18.83 — 修复 async noop + 空闲 HideUI（C-D10）

#### 改动（**不修改** `_ToggleAsciiMode` / S-B5 / T-B3）

1. 保留 `_PullResponseAfterKey`（防 style 覆盖）
2. 非空闲：设 `_ipc_refresh_requested` + `_composition_dirty`，**一律 sync** EditSession
3. 空闲且非组字：`_HideUI()`，不进 ES
4. `UpdateUI`：`!composing` 时 `Show(FALSE)` 关闭候选窗

#### 验收

- 第二字母起不卡死；候选窗可正常消失
- 快捷键与组字 shift-pick 仍 PASS
- 微信/Cursor 持续输入稳定性待测

**多宿主铁证 @ 0.18.83**：
- 微信 14:45:24–45 输入正常（session=6，shift-pick/toggle PASS）
- **14:48:00** Cursor `ActivateEx`（新开宿主）
- **14:48:07** 微信崩溃 `sess=6 depth=1`（微信当时未在输入）
- **结论**：与多宿主焦点切换相关；`OnKillThreadFocus` 同步 `_EndComposition(clear)` 与 Cursor 激活竞态

### 0.18.84 — 多宿主焦点隔离（M-B1）

#### 改动（**保留** C-D10 key-pull / T-B3 / S-B5）

1. `OnKillThreadFocus`：先 `_HideUI`；异步 `_QueueEndCompositionEditSession`（**不再**同步 `_EndComposition(clear)`）
2. `ClearComposition` 包 try/catch；清理 pending/dirty 标志
3. `_AfterRimeKey` idle：无论 `_IsComposing()` 均 `_HideUI` + 异步结束 TSF 组字
4. `OnSetFocus`：增加 `focus in/out` 诊断 crumb

#### 验收

- 微信输入后打开 Cursor：**微信不闪退**
- 候选窗随失焦/空闲正常消失
- 快捷键与组字 shift-pick 仍 PASS

**仅微信铁证 @ 0.18.84**：
- `14:58:09` `host=Weixin.exe ver=0.18.84`；崩溃前同秒大量 `key-ipc es sync` + **成对** `edit-session begin ipc`
- **结论**：`OnTestKeyDown`+`OnKeyDown` 双次 `_AfterRimeKey`（`pfEaten=0` 时）+ 持续组字一律 sync → IPC/ES 风暴

### 0.18.85 — 去双键路径 + 组字 async（C-D12）

#### 改动（**保留** C-D10 key-pull / M-B1 / T-B3 / S-B5）

1. `KeyEventSink`：`OnTestKeyDown/Up` 仅 `ProcessKeyEvent` 测键；**仅** `OnKeyDown/Up` 调用 `_AfterRimeKey`（Test 已处理则跳过二次 ProcessKeyEvent）
2. `_AfterRimeKey`：commit 或首键开组字 sync；持续组字 **async** + `_ipc_refresh_requested`；pending ES 时 coalesce `_pending_key_pull`
3. `OnKillThreadFocus`：清理 test-key 标志

#### 验收

- 微信**仅微信**连续输入 ≥30 分钟不崩
- 日志：`edit-session` 不成对刷屏；组字期见 `es async` 而非全 `es sync`
- 第二字母不卡死；候选窗可见且可消失
- 快捷键与组字 shift-pick 仍 PASS
- 微信输入后开 Cursor 不崩（M-B1 仍要验）

**0.18.85 铁证 @ 用户验收**：
- 微信首输 `weasel.ui.log`：`font=0 size=0x0 candies=5`（会话 Echo 复用、`__synced` 已真，pipe 无 style）
- Cursor 后开正常 `font=14`（新 TSF 实例灌入 style）
- `15:08:25` 重启算法服务 → Cursor+微信同秒 `0xE06D7363`
- `15:11` 段 `edit-session` 仍成对 → **KeyDown+KeyUp 双次 AfterRimeKey**（C-D12 未堵住 KeyUp 路径）

### 0.18.86 — KeyUp 去重 + style 灌入（C-D13）

#### 改动（**保留** C-D12/M-B1/T-B3/S-B5）

1. `KeyEventSink`：KeyDown 已 `_AfterRimeKey` 则 KeyUp **跳过**（shift-pick/toggle 仍走 KeyUp）
2. `_EnsureStyleFromSession`：font=0 时 EndSession+StartSession 消费 AddSession pipe 灌 style；ActivateEx/Reconnect/首键前调用
3. `KeyPullCache` 缓存 `UIStyle`；EditSession 恢复 style 防 coalesce 丢失

#### 验收

- 安装后**先开微信**打 `nihao`：候选窗可见（`font=14`）
- 日志：无连续成对 `edit-session`；无 `font=0 size=0x0`
- 重启算法服务后：不立即双崩；微信/Cursor 连续输入稳定
- 快捷键与 shift-pick 仍 PASS

**暂停后首键铁证 @ 0.18.86**：
- `15:21:42` 最后输入 `composing=0`；`15:23:50` 暂停 ~2min 后首键崩 `depth=1 composing=0 sess=1`
- 崩溃前无新 `key-ipc` crumb → 陈旧 `_pComposition` / 异步 idle 收尾未完成
- 连续输入段正常（C-D13 有效）

### 0.18.87 — 空闲 sync 收尾组字（C-D14）

#### 改动（**保留** C-D13/M-B1/T-B3/S-B5）

1. `_AfterRimeKey` idle：改 **sync** `CEndCompositionEditSession`（不再 async QueueEnd）；清 pending pull/ES
2. `_ResetStaleCompositionState`：首键前复位卡住的 `_end_composition_pending`
3. `need_sync` 增加 `stale_comp`（TSF 与 server composing 不一致）
4. `_Reconnect`：Finalize 残留 TSF 组字

#### 验收

- 微信连续输入不崩；**暂停 ≥2 分钟后首键不崩**
- 候选窗仍可见；快捷键/shift-pick PASS

### 0.18.88 — 按键/响应管线重构（R-B1）

#### 改动（行为等价 + 结构简化）

1. `KeySinkState`：8 个散落 BOOL 合并为单结构体 + `ResetDown/Up/All`
2. `_AfterRimeKey` 拆为：`_HandleIdleKeyResponse`、`_RequestKeyEditSession`、`_CompositionStaleMismatch`
3. 移除 async **coalesce**（`_pending_key_pull` 仅在 EditSession 消费残留）；组字仍可按策略 async/sync
4. 保留 C-D13/C-D14 全部已证真行为

#### 验收

- 同 C-D14 门控；代码可读性提升便于 WinDbg 后定点修改
