# Changelog

本文件记录 **LittleCraneRime（小鹤输入法）** 仓库的版本变更历史。

> 上游变更历史请参考 [rime/weasel CHANGELOG](https://github.com/rime/weasel/blob/master/CHANGELOG.md)。
> 本仓库 fork 自 [rime/weasel 0.17.4](https://github.com/rime/weasel/releases/tag/0.17.4)（上游 2025-06-04 发布的最新稳定版）。
> 本仓库的版本号（如 `0.18.100`）是 fork 自己的迭代号，与上游版本号无直接对应关系。

---

## [0.18.100] - 2026-06-15

### 重大功能（让 Rime 更易用）

#### 快速设置栏（Quick Settings Bar）
- 新增可视化快速设置栏（`WeaselServer/TrayQuickBar.*`）
- **Alt + ,** 快捷键调出，托盘区"词典"按钮也可进入
- 内含：快捷键设置、方案切换设置、UI 风格设置、固定短语、常用短语、词典管理等

#### 内置双拼方案
- 在内置方案中新增 7 套双拼方案：`output/data/double_pinyin*.schema.yaml`
- 包括：小鹤双拼（flypy）、微软双拼（mspy）、搜狗双拼（sogou）、智能 ABC、加加、紫光、自然码

#### 自定义固定短语可视化编辑
- `WeaselDeployer/CustomPhraseDialog.*`、`CustomPhraseListDialog.*`
- 可视化增删改，支持快捷导入/导出

#### 常用短语（可视化 + 快捷调用）
- `WeaselServer/TrayCommonPhrasePanel.*`、`WeaselDeployer/CommonPhraseEditDialog.*`
- **Alt + .** 调出，鼠标双击或方向键 + 回车上屏

#### 快捷键可视化设置
- `WeaselDeployer/HotkeySettingsDialog.*`、`KeyCaptureEdit.*`
- 图形化重新绑定常用快捷键

#### Shift 中英文 / 中英文标点切换
- `include/ShiftComboGuard.h`、`WeaselTSF/Composition.cpp`、`WeaselTSF/KeyEvent.cpp`
- Shift 单独松开切换中英（或在标点模式下切换中英标点）
- 修复"切到英文后第一个键又被翻回中文"的问题：1.5s 锁窗口内持续 force-sync

### 稳定性改进

#### TSF 多宿主异常防御
- `include/WeaselEditSessionSafe.h`、`WeaselTSFCallbackSafe.h`
- 钉钉 / 飞书 / 微信 / Cursor 等宿主下 C++ 异常不再传播

#### VEH Crash 日志节流
- `include/WeaselCrashDiag.h`
- 线程级每 1000ms 最多 1 次；全局级每 500ms 最多 1 次
- 真实崩溃不受节流影响
- 实测：`weasel.crash.log` 1 小时增长 < 1MB（原 92MB）

#### 安装器强化
- Restart Manager 强制关闭持有 DLL 句柄的进程
- `Delete /REBOOTOK` 兜底覆盖所有 DLL/IME/exe
- `weasel.crash.log` 64MB 自动轮转

---

## [0.18.99 → 0.18.100] 试错历史（保留可追溯）

> 完整记录见 [`docs/FIX_ATTEMPTS.md`](docs/FIX_ATTEMPTS.md)

### [0.18.99] - 2026-06-15

- 修复 x64/x86 二进制覆盖（NSIS `/oname=output\Win32\` 显式子目录）
- `WeaselServer.vcxproj` 加 trailing-backslash
- 强化 TSF EditSession 异常防御

### [0.18.98]

- 继续加固 IPC 重连时的状态机

### [0.18.97]

- 引入 lock-rime 机制（修复 Shift 切换"反弹"）
- 引入 Restart Manager 调用

### [0.18.96]

- 移除部分补丁后出现 Shift 切换失效（已被 0.18.97 修复）

### [0.18.95]

- 安装路径检测优化
- IPC 重连后 `app_options` 应用修复

### [0.18.92]

- 安装流程加固
- 多宿主 TSF C++ 异常捕获 (L-TSF)

### [0.18.91]

- TSF 回调异常防御 (C-TSF)
- Cursor、飞书 兼容

### [0.18.89]

- **R-B1 重构**：键事件管线简化
- 移除 async coalesce
- 新增 `KeySinkState` 单一结构体
- 验证：连续输入 + 暂停 2min 首键 + Cursor/微信交替

### [0.18.87]

- `_HandleIdleKeyResponse` / `_RequestKeyEditSession` 拆分
- 合并 C-D14 idle sync 收尾

### [0.18.86]

- **C-D13 风格灌入**：候选框可见性修复
- 微信连续输入不崩（Weixin.exe 0xE06D7363 防御）
- `KeyUp` 跳过已处理

### [0.18.85]

- 候选框 font_point 自适应
- 全屏/竖排布局修正

### [0.18.83]

- **C-D10 key-pull + sync 输入**（证真）
- 同步管线（无 async）作为基本架构

### [0.18.78]

- **T-B3 Shift 单独释放切换**（证真 PASS）— 关键里程碑
- **S-B5 Shift+组合键防误触**（证真 PASS）— 引入 `ShiftComboGuard`

### [0.18.75]

- Shift 切换第一次 commit 修复尝试（部分方案下失效）

### [0.18.70]

- Shift 切换问题最早记录
- 多宿主崩溃问题最早记录（飞书、Cursor）

### [0.18.69]

- 飞书、Cursor 首次报告崩溃（0xE06D7363）

### [0.18.52 - 0.18.68]

- 上游 `rime/weasel 0.17.x` ~ `0.18.50` 的滚动同步
- 本仓库从此基线分叉

---

## 关键里程碑

| 版本 | 里程碑 |
|------|--------|
| 0.18.78 | **T-B3** Shift 切换证真 |
| 0.18.86 | **C-D13** 风格灌入；微信不崩 |
| 0.18.89 | **R-B1** 键事件管线重构 |
| 0.18.99 | x64/x86 区分；NSIS 子目录 |
| 0.18.100 | VEH 节流；Shift lock 优化 |

---

## 与上游同步策略

- 上游 master 分支有新 commit → cherry-pick 应用本仓库
- 上游新版本发布 → 评估差异，必要时升级基线
- 当前 fork 基线：`rime/weasel 0.17.4`（2025-06-04）
- 上游 master 升级 → 优先同步 `WeaselTSF/` 和 `WeaselUI/`（UI/TSF 改动），保留本仓库的 `WeaselCrashDiag.h`、`ShiftComboGuard.h`、`WeaselEditSessionSafe.h` 等

---

## 致谢

- 上游 [rime/weasel](https://github.com/rime/weasel)
- 引擎 [rime/librime](https://github.com/rime/librime)
- 所有 RIME 社区贡献者
