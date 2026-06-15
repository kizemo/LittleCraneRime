# 小鹤输入法 · LittleCraneRime

[![版本 0.18.100](https://img.shields.io/badge/version-0.18.100-blue.svg)](https://github.com/kizemo/LittleCraneRime/releases)
[![平台](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64%2Bx86-blue.svg)]()
[![基于](https://img.shields.io/badge/fork-rime%2Fweasel-green.svg)](https://github.com/rime/weasel)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-orange.svg)](LICENSE)

> 基于 [rime/weasel](https://github.com/rime/weasel) 的 Windows 小狼毫输入法分叉版本。
> 主旨：让 Rime 这款轻量、强大的输入法拥有更友好的可视化设置体验，更适合普通用户上手使用。

---

## 项目背景

本仓库并非直接克隆自 GitHub 上的 [rime/weasel](https://github.com/rime/weasel) 原始版本，而是基于一个已经使用 **Cursor** AI 做了大量二次开发修改的本地版本。
在二次开发过程中遇到的一些崩溃、不稳定现象，主要是 Cursor 自动编辑过程中引入的问题，**并非原版 weasel 项目本身的缺陷**。

### 为什么要做这次二次开发？

Rime 输入法是一款非常优秀的开源输入法引擎，轻量、功能强大、可定制性极高。但它的使用门槛相对较高：

- **配置复杂**：很多常用设置需要手动修改 `yaml` 文件，普通用户难以驾驭
- **缺少可视化界面**：常见的快捷键设置、配色调整、词库管理等都没有图形化的设置面板
- **内置方案有限**：原版内置方案中没有双拼方案

针对这些痛点，本仓库在保留 Rime 原有强大功能的基础上，重点增强了**可视化、易用性**，让不熟悉配置文件编辑的普通用户也能轻松使用。

---

## 核心特性（重点是让 Rime 更易用）

### 1. 快速设置栏

新增**可视化快速设置栏**，对常用功能提供一键式设置入口：

- 通过 **Alt + ,** 快捷键即可快速调出快速设置栏
- 通过输入法托盘区的"词典"按钮也可进入设置界面
- 设置项支持快速修改，并可一键导入、导出用户配置
- 设置内容涵盖：快捷键、配色方案、常用短语、固定短语、词典管理等

这一栏的核心目标是：**让用户不再需要手动编辑 `yaml` 文件**。

相关实现：`WeaselServer/TrayQuickBar.cpp`、`WeaselDeployer/*.cpp` 等。

---

### 2. 内置双拼方案

原版 weasel 内置方案中并没有双拼方案。本仓库修改了内置输入方案，**增加了多种常用双拼方案**：

- 小鹤双拼（flypy）
- 微软双拼（mspy）
- 搜狗双拼（sogou）
- 智能 ABC 双拼
- 加加双拼（jiajia）
- 紫光双拼（ziguang）
- 自然码双拼

相关文件：`output/data/double_pinyin*.schema.yaml`。

---

### 3. 自定义固定短语（可视化编辑）

为方便用户管理自定义短语，提供**完全可视化的固定短语编辑界面**：

- 通过 **Alt + ,** 调出快速设置栏 → "词典"按钮 → 进入固定短语编辑
- 支持添加、修改、删除固定短语
- **支持快捷导入、导出**用户的固定短语文件，方便在不同电脑间同步或备份

相关实现：`WeaselDeployer/CustomPhraseDialog.cpp`、`WeaselDeployer/CustomPhraseListDialog.cpp`。

---

### 4. 常用短语（可视化编辑 + 快捷调用）

新增**常用短语**功能，使用可视化界面编辑，并通过快捷键快速调用：

- 使用 **Alt + .** 快捷键可快速调出常用短语面板
- 常用短语以可视化列表呈现，可随时通过界面编辑
- 鼠标**双击**短语，或通过**方向键选中 + 回车**即可快速上屏
- 适用于邮箱、地址、签名等高频重复内容

相关实现：`WeaselServer/TrayCommonPhrasePanel.cpp`、`WeaselDeployer/CommonPhraseEditDialog.cpp`。

---

### 5. Shift 快捷切换中英文 / 中英文标点

在保留 Rime 原有切换方式的同时，**增加了 Shift 单独按下的复合切换能力**：

- **Shift 单独松开**：切换中 / 英文输入模式
- **Shift 单独松开（在标点模式下）**：切换中英文标点
- Shift + 字母 / Shift + Ctrl + 字母等组合场景不会被误触
- 切换稳定、可靠，避免出现"切到英文后第一个键又被翻回中文"的现象

相关实现：`WeaselTSF/Composition.cpp`、`WeaselTSF/KeyEvent.cpp`、`include/ShiftComboGuard.h`。

---

### 6. 快捷键可视化设置

通过快速设置栏的"**快捷键设置**"按钮，可以**可视化修改**常用功能的快捷键：

- 中英文切换快捷键
- 中英文标点切换快捷键
- 调出常用短语 / 固定短语的快捷键
- 其他常用功能快捷键

无需手动编辑 `default.yaml`，全部通过图形界面完成。

相关实现：`WeaselDeployer/HotkeySettingsDialog.cpp`、`WeaselDeployer/KeyCaptureEdit.cpp`、`include/WeaselHotkeyConfig.h`。

---

## 其他改进（基于二次开发过程中的稳定性调整）

> 以下改进是在二次开发过程中，对已知的不稳定因素进行的修补与加固，使整体使用体验更稳定。

### 1. TSF 多宿主异常防御

针对钉钉、飞书、微信、Cursor 等大型 TSF 宿主环境下偶发的 C++ 异常传播问题，增加多层异常捕获与转换：

- `EditSession` 异常防御（`include/WeaselEditSessionSafe.h`）
- TSF 回调异常防御（`include/WeaselTSFCallbackSafe.h`）
- 异常被吞并转换为 `E_FAIL`，不会让宿主进程崩溃

### 2. VEH 异常诊断与日志节流

为防止 `weasel.crash.log` 在异常情况下无限膨胀，增加节流机制：

- VEH 记录 0xE06D7363（C++ 异常）等情况
- 线程级与全局级双重节流
- 真实崩溃（access violation 等）不被节流，永不丢失

相关实现：`include/WeaselCrashDiag.h`。

### 3. Shift 切换可靠性优化

针对 Shift 单独释放切换中英文后，少数场景下第一个键又被引擎翻回中文的问题，进行修复：

- 在 lock 窗口（1.5s）内持续 force-sync
- 锁窗口外且模式匹配时才释放 lock

### 4. 安装器自我修复

- 通过 Windows Restart Manager 强制关闭持有 DLL 句柄的进程
- 修复 x64 / x86 二进制互相覆盖的打包问题
- 升级时保护用户配置目录不被覆盖

---

## 默认配置详解

> 配置文件位置（默认）：`%APPDATA%\Rime\`（即 `C:\Users\<user>\AppData\Roaming\Rime\`）

### 1. 全局配置（`weasel.yaml`）

```yaml
config_version: "2024-09-25"     # 配置 schema 版本

app_options:
  firefox.exe:
    inline_preedit: true          # 修复 Firefox 内预编辑丢失

show_notifications: true          # 状态变化时显示通知
show_notifications_time: 1200     # 通知停留 ms
global_ascii: false               # 切换 ascii 是否全局（false=逐应用）

style:
  color_scheme: purity_of_form_custom  # 默认配色
  font_point: 14                       # 候选字号
  inline_preedit: true                 # 行内预编辑
  horizontal: true                     # 候选横排
  fullscreen: false                    # 非全屏
  corner_radius: 8                     # 圆角 8px
  display_tray_icon: false             # 语言栏图标（不托盘）
```

### 2. 常用快捷键

> 常用快捷键可在**快速设置栏 → 快捷键设置**中可视化修改，无需手动编辑配置文件。

#### 2.1 中英文切换

| 行为 | 快捷键 | 说明 |
|------|--------|------|
| **中/英切换** | **Shift 单按松开** | 默认推荐使用，松手才触发，按住不触发 |
| 中/英切换 | `Caps Lock` | 兼容老式方案 |
| 中/英切换 | `Control+Space` | 传统方案 |

#### 2.2 模式切换

| 行为 | 快捷键 | 说明 |
|------|--------|------|
| 切换输入方案 | `Control+Shift+F4` / `F4` | 在多方案（含双拼）之间切换 |
| 切换简化/繁体 | `Control+Shift+F3` / `F3` | 方案相关 |
| 切换半角/全角 | `Shift+Space` | |
| 切换中英标点 | `Control+.` | |

#### 2.3 编辑与翻页

| 行为 | 快捷键 |
|------|--------|
| 上屏当前候选 | `Space` 或 `1-9` 数字 |
| 上一候选 | `Shift+Tab` 或 `−` |
| 下一候选 | `Tab` 或 `=` |
| 上一页候选 | `−`、`PageUp` |
| 下一页候选 | `=`、`PageDown` |
| 删除当前输入 | `Esc` |
| 退格 | `BackSpace` |
| 翻到第一/最后 | `Home` / `End` |
| **快速选第二/三候** | `Alt+数字` |

#### 2.4 本仓库新增的常用快捷键

| 行为 | 快捷键 | 说明 |
|------|--------|------|
| **调出快速设置栏** | **Alt + ,** | 新增 |
| **调出常用短语面板** | **Alt + .** | 新增 |
| 切换中英文（Shift） | `Shift` 单独松开 | 新增优化 |

> 上述"新增"的快捷键，均可在快速设置栏的可视化界面中按个人喜好重新设置。

### 3. RIME 引擎快捷键（`default.yaml`）

```yaml
# 默认 patch（在 patch 块下，merge 进 rime_ice 或 rime 默认 schema）
patch:
  key_binder/bindings/+:
    - { when: composing, send: Shift+Right, accept: Tab }
    - { when: composing, accept: Alt+period, send: Escape }
    - { when: always, toggle: ascii_mode, accept: Control+space }

  ascii_composer/switch_key/Shift_L: noop   # 禁用 RIME 内 Shift 切换（由 TSF 处理）
  ascii_composer/switch_key/Shift_R: noop

  switcher:
    hotkeys: [Control+Shift+F4, F4, Alt+comma]
  key_binder:
    bindings:
      - { when: composing, accept: Alt+period, send: Escape }
```

### 4. 配色方案（30+ 内置）

`weasel.yaml` 中 `preset_color_schemes` 节包含：
- `aqua` 碧水、 `azure` 青天、 `luna` 明月、 `ink` 墨池
- `lost_temple` 孤寺、 `dark_temple` 暗堂
- `google`、`google_plus`、`solarized_rock`
- `nord`、`purity_of_form_custom`（默认）、`flypy` 小鹤飞扬
- `metro_blue`、`psionics`、`starcraft` 系列
- 30+ 主题，详见 `output/data/weasel.yaml`

修改默认：
```yaml
style:
  color_scheme: flypy              # 改为小鹤飞扬
  color_scheme_dark: nord          # 暗色模式用 nord
```

### 5. 应用专属配置（`weasel.yaml` `app_options`）

```yaml
app_options:
  firefox.exe:
    inline_preedit: true           # Firefox 内行内预编辑
  # 默认常见的命令行工具设为英文
  # cmd.exe: { ascii_mode: true }
  # powershell.exe: { ascii_mode: true }
  # wt.exe: { ascii_mode: true }
  # vim: { ascii_mode: true, vim_mode: true }   # 自动进入 vim 模式
```

---

## 安装与使用

### 系统要求

- Windows 10 1809 (build 17763) 或更高
- x64 与 x86 架构
- 推荐 Windows 11 22H2

### 安装

1. 前往 [Releases](https://github.com/kizemo/LittleCraneRime/releases) 下载最新 `weasel-x.x.x.x-installer.exe`
2. 以**管理员权限**运行
3. 安装路径默认 `C:\Program Files\Rime\weasel-x.x.x\`（建议不要改）
4. 用户数据目录默认 `%APPDATA%\Rime\`
5. 安装器会自动：
   - 关闭所有 Weasel 进程
   - 通过 Restart Manager 关闭持有 DLL 句柄的进程
   - 替换 `System32\` 和 `SysWOW64\` 中的 DLL/IME
   - 注册输入法到 TSF

### 升级

- 在已安装的旧版本上**直接运行新安装器**（覆盖安装）
- 用户数据目录 `%APPDATA%\Rime\` 不会被覆盖
- 升级后建议**重启所有应用程序**以加载新的 weaselx64.dll

### 上手使用建议

1. 安装完成后，调出输入法托盘菜单，先尝试使用 **Alt + ,** 调出快速设置栏
2. 在快速设置栏中浏览各项设置，熟悉可视化配置方式
3. 如有双拼需求，在方案切换中选择对应双拼方案
4. 高频重复内容，可通过常用短语（Alt + .）录入
5. 常用功能快捷键如有偏好，可在"快捷键设置"中可视化调整

---

## 构建（开发者）

### 依赖

- Visual Studio 2022 + Windows SDK 10.0.22621.0
- Boost 1.85+ (推荐预编译：`scripts\build-boost-x64.bat`)
- CMake 3.20+
- NSIS 3.x（仅打包）
- librime 1.13.1+（已在 `librime/` 子目录）

### 构建命令

```bat
:: 准备 Boost 依赖（首次）
scripts\build-boost-x64.bat
scripts\build-boost-win32.bat

:: 构建 0.18.100
scripts\build-0.18.100.bat

:: 重新打包（仅 NSIS）
scripts\pack-installer-0.18.99.bat
```

### 项目结构

```
weasel/
├── WeaselTSF/             # TSF 接口 (OnKeyDown, OnComposition, etc.)
├── WeaselServer/          # 守护进程，IPC 服务
│   ├── TrayQuickBar.*        # 快速设置栏（新）
│   └── TrayCommonPhrasePanel.* # 常用短语面板（新）
├── WeaselIPC/             # 命名管道 + 请求/响应协议
├── WeaselIPCServer/       # Server 端 IPC 处理
├── WeaselUI/              # DirectUI 候选框渲染
├── WeaselDeployer/        # 部署/同步工具、可视化设置面板
│   ├── CustomPhraseDialog.*         # 固定短语编辑（新）
│   ├── CustomPhraseListDialog.*     # 固定短语列表（新）
│   ├── CommonPhraseEditDialog.*     # 常用短语编辑（新）
│   ├── HotkeySettingsDialog.*       # 快捷键设置（新）
│   ├── SwitcherSettingsDialog.*     # 方案切换设置（新）
│   ├── UIStyleSettingsDialog.*      # UI 风格设置（新）
│   └── DictManagementDialog.*       # 词典管理（新）
├── WeaselSetup/           # 安装器
├── RimeWithWeasel/        # librime 适配层
├── include/               # 公共头
│   ├── CommonPhraseStore.h          # 常用短语存储（新）
│   ├── ShiftComboGuard.h            # Shift 防误触
│   ├── WeaselHotkeyConfig.h         # 快捷键配置（新）
│   ├── WeaselEditSessionSafe.h      # EditSession 异常防御
│   ├── WeaselTSFCallbackSafe.h      # TSF 回调异常防御
│   └── WeaselCrashDiag.h            # VEH 节流
├── scripts/               # 构建/打包/部署脚本
├── docs/                  # 设计文档
├── output/                # 编译产物
│   ├── data/                 # 用户配置 data/
│   └── double_pinyin*.yaml   # 内置双拼方案（新）
└── weasel.props           # 版本号 / 输出路径
```

---

## 与上游 rime/weasel 的差异

| 模块 | 上游 | 本仓库 | 说明 |
|------|------|--------|------|
| `WeaselDeployer` | 基础部署工具 | 新增 6 类可视化设置对话框 | 快速设置栏 |
| `WeaselServer` | 守护进程 | 新增快速设置栏 / 常用短语面板 | 托盘交互 |
| 内置方案 | 仅拼音 | 新增 7 套双拼方案 | flypy / mspy / sogou 等 |
| Shift 切换 | 单键触发 | 加锁窗口、组合键防误触 | |
| TSF 异常 | 无防御 | EditSession / TSF 回调 / VEH 节流 | 稳定性提升 |

详细差异见 `docs/PROJECT_REVIEW.md` 与 git log。

---

## 文档

- [CHANGELOG.md](CHANGELOG.md) — 版本变更记录
- [HOTKEYS.md](HOTKEYS.md) — 完整快捷键速查表
- [INSTALL.md](INSTALL.md) — 详细安装说明
- [docs/FIX_ATTEMPTS.md](docs/FIX_ATTEMPTS.md) — 试错记录
- [docs/SHIFT_COMBO_ANALYSIS.md](docs/SHIFT_COMBO_ANALYSIS.md) — Shift 切换的失效场景分析
- [output/data/weasel.yaml](output/data/weasel.yaml) — 默认 `weasel.yaml` 模板
- [output/data/default.yaml](output/data/default.yaml) — RIME 引擎默认配置

---

## 常见问题

### Q: 升级后 Shift 切换又失效？
A: 升级后请重启所有应用。新安装器更新磁盘上的 `weaselx64.dll`，但已在内存中加载的进程（Explorer、钉钉等）仍使用旧版本。重启进程即可。

### Q: 钉钉 / 飞书仍然卡顿？
A: 检查 `%TEMP%\rime.weasel\weasel.crash.log`。如果该文件快速增长，说明仍存在未被节流的真实异常（节流只针对 0xE06D7363）。提交 issue 并附上 crash log。

### Q: 安装时报"文件被占用"？
A: 安装器会通过 Restart Manager 自动关闭持有 DLL 句柄的进程。如果仍然失败，请先手动退出钉钉 / 微信 / 飞书 / Cursor。

### Q: 配色不生效？
A: 编辑 `%APPDATA%\Rime\weasel.yaml`，确认 `style/color_scheme` 字段拼写正确（区分大小写）。也可在快速设置栏中实时切换预览。

### Q: 如何回滚到原版 rime/weasel？
A: 在 <https://github.com/rime/weasel/releases> 下载官方安装器，覆盖安装即可。本仓库修改遵循 GPL-3.0，完全兼容。

### Q: 双拼方案如何切换？
A: 通过 `Control+Shift+F4` 或 `F4` 切换输入方案，在列表中选择对应的双拼方案（如"小鹤双拼"）。

---

## 贡献

欢迎 issue 与 PR！建议流程：

1. Fork 本仓库
2. 创建分支 `git checkout -b feature/your-feature`
3. 提交前先跑 `scripts\build-0.18.100.bat` 验证编译通过
4. PR 描述清楚：功能改进点、使用场景、回归测试

---

## 鸣谢

- [rime/weasel](https://github.com/rime/weasel) 上游项目
- [rime/librime](https://github.com/rime/librime) 引擎
- [Lotus](https://github.com/lotem) 创始与维护
- [Mirtle](https://github.com/mirtlecn) weasel.yaml 整理
- [fxliang](https://github.com/fxliang)、[居戎氏](https://github.com/jurongshijia) 等贡献者
- 所有 RIME 用户社区的支持

## 许可

本仓库继承上游 **GPL-3.0** 许可。详见 [LICENSE](LICENSE) 文件。
