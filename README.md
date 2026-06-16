# 小鹤输入法 · LittleCraneRime

[![版本 0.18.100](https://img.shields.io/badge/version-0.18.100-blue.svg)](https://github.com/kizemo/LittleCraneRime/releases)
[![平台](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64%2Bx86-blue.svg)]()
[![基于](https://img.shields.io/badge/fork-rime%2Fweasel-green.svg)](https://github.com/rime/weasel)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-orange.svg)](LICENSE)

> 一款面向「普通用户」的中文输入法分叉：在保留 [rime/weasel](https://github.com/rime/weasel) 全部核心能力的基础上，补齐可视化管理界面、扩展内置方案、修复已知稳定性问题，让 Rime 从「极客玩具」真正变成「开箱即用的日常工具」。

---

## 这是一款什么样的项目

Rime（中州韵、小鹤、小企鹅等平台实现）是一款由 [佛振](https://github.com/lotem) 主导开发的开源输入法引擎。它的设计哲学强调**配置即一切**：通过几份 `yaml` 配置文件，用户可以自由地定义输入法的一切行为——这种高度可定制性，恰恰也是它最让普通用户望而却步的地方。

**LittleCraneRime** 关注的是与原版 weasel 互补的另一个方向：把那些必须靠手写 yaml 才能调整的常用选项（快捷键、配色、词库、短语、方案切换）封装成**直观的图形界面**，并补齐原版没有的功能（双拼方案、常用短语、Shift 单按切换等）。

本仓库的修改来源主要有三层：

- **基线版本**：fork 自 [rime/weasel 0.17.4](https://github.com/rime/weasel/releases/tag/0.17.4)（上游 2025-06-04 发布的最新稳定版）
- **本仓库版本号**：0.18.100——这里的版本号是 fork 自己的迭代号，**与上游版本号无直接对应关系**
- **本地增强**：基线之上的可视化能力、可视化设置交互，由作者结合 AI 辅助编辑器在本地迭代实现
- **稳定性修订**：TSF 多宿主异常防御、Shift 切换可靠性、安装器自我修复等（详见后文）

> ⚠️ **关于稳定性修订的说明**：早期本地开发中曾依赖 AI 编辑器进行大量改动，过程中部分边界场景曾暴露出稳定性问题。本仓库的稳定性相关修订，正是对这一阶段积累问题的回归与修复——并不代表上游 rime/weasel 本身存在这些缺陷，更多是迭代过程的整合与打磨。

---

## 相对上游 rime/weasel 的主要差异

下表给出本仓库**相对上游 0.17.4 的可感知差异**，按"用户可感知"程度由高到低排序。

### 1. 全新可视化设置体系（用户可感知度：⭐⭐⭐⭐⭐）

| 模块 | 入口 | 文件 | 能力 |
|------|------|------|------|
| 快速设置栏 | `Alt + ,` 快捷键 / 托盘"词典"按钮 | `WeaselServer/TrayQuickBar.*` | 浮窗式快速入口：方案 / 快捷键 / 固定短语 / 常用短语 |
| 常用短语面板 | `Alt + .` 快捷键 | `WeaselServer/TrayCommonPhrasePanel.*` | 列出常用短语，方向键 / 鼠标双击 / 回车上屏；带键盘钩子 |
| 快捷键设置 | 快速设置栏 → 快捷键 | `WeaselDeployer/HotkeySettingsDialog.*` + `KeyCaptureEdit.*` | 17 项常用快捷键可视化绑定，含冲突检测 |
| 方案切换设置 | 快速设置栏 → 方案 | `WeaselDeployer/SwitcherSettingsDialog.*` | 复选式启用 / 调整每个方案的热键 |
| UI 风格设置 | 快速设置栏 → 风格 | `WeaselDeployer/UIStyleSettingsDialog.*` + `UIStyleSettings.*` | 配色方案选择 + 实时预览 |
| 自定义固定短语 | 快速设置栏 → 固定短语 | `WeaselDeployer/CustomPhraseDialog.*` + `CustomPhraseListDialog.*` | 可视化增删改、**导入 / 导出**、同步用户词典 |
| 常用短语编辑 | 快速设置栏 → 常用短语 | `WeaselDeployer/CommonPhraseEditDialog.*` | 常用短语的可视化编辑界面 |
| 词典管理 | 快速设置栏 → 词典 | `WeaselDeployer/DictManagementDialog.*` | 用户词典的备份 / 还原 / 导入 / 导出 |

以上对话框**全部为新增模块**，上游 weasel 仅提供一个简单的命令行式 `WeaselDeployer`，绝大多数配置项必须手工编辑 `weasel.yaml` / `default.yaml`。本仓库把这部分用户体验整体重做。

### 2. 快捷键配置可视化与可热加载

新增 `include/WeaselHotkeyConfig.h` 头文件，提供：

- `weasel_hotkeys.yaml` 用户级配置文件，**无需重启输入法**即可热加载
- `MatchVkHotkey` / `FormatVkChord` 一套统一的快捷键匹配 / 规范化 API
- 内置历史默认值迁移逻辑：旧版本硬编码的 `Control+Shift+F` / `Control+Shift+K` 在用户未显式自定义时，自动迁移到新默认值 `Alt+comma` / `Alt+period`

### 3. Shift 单按切换中英文 / 中英文标点

新增 `include/ShiftComboGuard.h`，实现 **T-B3 算法**：

- **Shift 单独松开** → 触发中英文切换（或在标点模式下切换中英标点）
- **Shift + 字母 / Shift + Ctrl + 字母** 等组合键**不会**误触
- 切换后 1.5s 锁窗口内持续 force-sync，**修复"切到英文后第一个键又被翻回中文"的问题**
- 完全 TSF 层实现，不依赖全局键盘钩子，性能与兼容性兼顾

### 4. 内置 7 套双拼方案

原版 weasel 内置方案中**没有双拼**。本仓库补齐了以下方案（位于 `output/data/`）：

| 方案 | 文件 | 说明 |
|------|------|------|
| 小鹤双拼 | `double_pinyin_flypy.schema.yaml` | 国内最广泛的小鹤双拼 |
| 微软双拼 | `double_pinyin_mspy.schema.yaml` | Windows 自带方案 |
| 搜狗双拼 | `double_pinyin_sogou.schema.yaml` | 搜狗智能输入法方案 |
| 智能 ABC | `double_pinyin_abc.schema.yaml` | 经典 ABC 风格 |
| 加加双拼 | `double_pinyin_jiajia.schema.yaml` | 加加输入法方案 |
| 紫光双拼 | `double_pinyin_ziguang.schema.yaml` | 紫光拼音方案 |
| 自然码 | `double_pinyin.schema.yaml` | 自然码双拼 |

通过 `F4` / `Ctrl+Shift+F4` 即可在所有内置方案（含全拼 / 双拼）间切换。

### 5. 常用短语存储与编辑

新增 `include/CommonPhraseStore.h`：

- 常用短语存储在 `%APPDATA%\Rime\weasel_common_phrases.txt`（UTF-8 编码）
- 每行一条短语，**无需编码**——这是与固定短语（需要输入触发码）最大的区别
- 适用于邮箱、地址、签名、手机号、身份证等高频重复内容
- 提供 `LoadPhrases` / `SavePhrases` 公共 API，UI 与存储完全解耦

### 6. 稳定性与可靠性修订

> 这些修订更多体现在工程实现细节上，普通用户的使用感受是"输入法更稳了"，但它们都是为了消除早期迭代中累积的问题。

#### 6.1 TSF 多宿主异常防御

针对钉钉、飞书、微信、Cursor 等大型 TSF 宿主下偶发的 C++ 异常传播问题：

- `include/WeaselEditSessionSafe.h`：`EditSession` 包装为 `RunEditSessionSafe`，C++ 异常转 SEH、SEH 转 HRESULT
- `include/WeaselTSFCallbackSafe.h`：`OnKeyDown` / `OnTestKeyDown` / `OnSetThreadFocus` 等回调包装在 `RunTSFCallback` 中
- 异常被吞并转换为 `E_FAIL`，**不会让宿主进程崩溃**

#### 6.2 VEH 异常诊断与日志节流

`include/WeaselCrashDiag.h`：

- 捕获 `0xE06D7363`（C++ 异常）、`0x80000003`（断点）等典型异常
- 线程级（1000ms）与全局级（500ms）双重节流
- **真实崩溃（access violation 等）不受节流影响**，永不丢失
- 实测：`weasel.crash.log` 1 小时增长 < 1MB（曾出现过的 92MB → 0.5MB）

#### 6.3 安装器自我修复

`WeaselSetup/process_closer.*` + NSIS 脚本：

- 通过 Windows Restart Manager 枚举持有 `weaselx64.dll` 句柄的进程并强制关闭
- `Delete /REBOOTOK` 兜底覆盖所有 DLL/IME/exe
- 修复 NSIS 打包时 x64 二进制被 x86 覆盖的历史 bug
- 升级时保护用户配置目录不被覆盖

#### 6.4 64MB Crash 日志自动轮转

防止 `weasel.crash.log` 在异常风暴下无限膨胀至 GB 级，引入自动轮转。

---

## 快速上手

### 系统要求

- Windows 10 1809 (build 17763) 或更高
- x64 与 x86 架构
- 推荐 Windows 11 22H2

### 安装

1. 前往 [Releases](https://github.com/kizemo/LittleCraneRime/releases) 下载最新 `weasel-x.x.x.x-installer.exe`
2. 以**管理员权限**运行
3. 安装路径默认 `C:\Program Files\Rime\weasel-x.x.x\`
4. 用户数据目录默认 `%APPDATA%\Rime\`

### 三步上手

1. **Alt + ,** 调出快速设置栏，先浏览一遍各项设置
2. 喜欢双拼？**F4** 切到"小鹤双拼"等方案
3. 邮箱 / 地址 / 签名反复输入？**Alt + .** → "编辑"加几条常用短语，之后双击或回车上屏

### 升级

- 在旧版本上**直接运行新安装器**（覆盖安装）
- 用户配置目录不会被覆盖
- 升级后建议**重启所有应用**以加载新版 `weaselx64.dll`

---

## 完整快捷键参考

> 以下默认快捷键**全部**可在"快速设置栏 → 快捷键设置"中可视化重新绑定，写入 `%APPDATA%\Rime\weasel_hotkeys.yaml`，无需重启。

### 中英文 / 标点切换（本仓库重点维护）

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| **中↔英切换** | **Shift 单按松开** | 推荐使用，1.5s 内 force-sync 防反弹 |
| 中↔英切换 | `Caps Lock` | 兼容老用户 |
| 中↔英切换 | `Ctrl+Space` | 传统方案 |
| 中↔英标点 | `Ctrl+.` | rime default.yaml |
| 半↔全角 | `Shift+Space` | rime default.yaml |

### 输入方案切换

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 唤出方案菜单 | `F4` 或 `Ctrl+Shift+F4` | 在所有内置方案（含双拼）间切换 |
| 唤出方案菜单 | `Alt+~` | 上游默认 |
| 直接选方案 | 数字键 `1-9` | 方案菜单中按数字 |

### 本仓库新增的快捷键

| 操作 | 快捷键（默认） | 说明 |
|------|--------|------|
| **调出快速设置栏** | **Alt + ,** | 浮窗式快速入口 |
| **调出常用短语面板** | **Alt + .** | 列表 + 方向键 / 双击 / 回车上屏 |

### 编辑键

| 操作 | 快捷键 |
|------|--------|
| 退格 | `BackSpace` |
| 删除当前输入 | `Esc` |
| 翻到首/末候选 | `Home` / `End` |
| 上一/下一候选 | `Shift+Tab` / `Tab` |
| 上一页/下一页候选 | `-` / `=` 或 `PageUp` / `PageDown` |
| 上屏第 1-9 候选 | `1` - `9` |
| 上屏当前候选 | `Space` |
| 快速选第二/三候 | `Alt+数字`（rime_ice） |

完整列表见 [HOTKEYS.md](HOTKEYS.md)。

---

## 默认配置

> 配置文件位置：`%APPDATA%\Rime\`

### 全局配置（`weasel.yaml`）

```yaml
config_version: "2024-09-25"

app_options:
  firefox.exe:
    inline_preedit: true

show_notifications: true
show_notifications_time: 1200
global_ascii: false

style:
  color_scheme: purity_of_form_custom
  font_point: 14
  inline_preedit: true
  horizontal: true
  fullscreen: false
  corner_radius: 8
  display_tray_icon: false
```

### 配色方案（30+ 内置）

`weasel.yaml` 中 `preset_color_schemes` 节包含：`aqua` 碧水、`azure` 青天、`luna` 明月、`ink` 墨池、`lost_temple` 孤寺、`dark_temple` 暗堂、`google`、`google_plus`、`solarized_rock`、`nord`、`purity_of_form_custom`（默认）、`flypy` 小鹤飞扬、`metro_blue`、`psionics`、`starcraft` 系列等。详见 `output/data/weasel.yaml`。

### 应用专属配置（`weasel.yaml` `app_options`）

```yaml
app_options:
  firefox.exe:
    inline_preedit: true
  # 命令行工具默认英文
  # cmd.exe: { ascii_mode: true }
  # powershell.exe: { ascii_mode: true }
  # wt.exe: { ascii_mode: true }
  # vim: { ascii_mode: true, vim_mode: true }
```

---

## 安装与构建（开发者）

### 系统要求

- Visual Studio 2022 + Windows SDK 10.0.22621.0
- Boost 1.85+（推荐预编译：`scripts\build-boost-x64.bat`）
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
├── WeaselTSF/                # TSF 接口（OnKeyDown、OnComposition 等）
├── WeaselServer/             # 守护进程 + IPC 服务
│   ├── TrayQuickBar.*            # 快速设置栏（浮窗）  ★ 新增
│   └── TrayCommonPhrasePanel.*   # 常用短语面板      ★ 新增
├── WeaselIPC/                # 命名管道 + 请求/响应协议
├── WeaselIPCServer/          # Server 端 IPC 处理
├── WeaselUI/                 # DirectUI 候选框渲染
├── WeaselDeployer/           # 部署 / 同步工具 + 全部可视化设置面板  ★ 大幅扩展
│   ├── HotkeySettingsDialog.*    # 快捷键设置
│   ├── SwitcherSettingsDialog.*  # 方案切换设置
│   ├── UIStyleSettingsDialog.*   # UI 风格设置
│   ├── CustomPhraseDialog.*      # 固定短语编辑
│   ├── CustomPhraseListDialog.*  # 固定短语列表
│   ├── CommonPhraseEditDialog.*  # 常用短语编辑
│   ├── KeyCaptureEdit.*          # 快捷键捕获控件
│   └── DictManagementDialog.*    # 词典管理
├── WeaselSetup/              # 安装器
├── RimeWithWeasel/           # librime 适配层
├── include/                  # 公共头
│   ├── WeaselHotkeyConfig.h      # 快捷键热加载配置
│   ├── CommonPhraseStore.h       # 常用短语存储
│   ├── ShiftComboGuard.h         # Shift 防误触（T-B3）
│   ├── WeaselEditSessionSafe.h   # EditSession 异常防御
│   ├── WeaselTSFCallbackSafe.h   # TSF 回调异常防御
│   └── WeaselCrashDiag.h         # VEH 节流
├── scripts/                  # 构建 / 打包 / 部署脚本
├── docs/                     # 设计文档
├── output/
│   ├── data/                 # 用户配置 data/
│   │   └── double_pinyin*.schema.yaml   # 内置双拼方案
│   ├── weasel.dll
│   ├── weaselx64.dll
│   ├── WeaselServer.exe
│   └── WeaselSetup.exe
└── weasel.props              # 版本号 / 输出路径
```

---

## 常见问题

**Q: 升级后 Shift 切换又失效？**
A: 升级后请重启所有应用。已在内存中加载的进程（Explorer、钉钉等）仍使用旧版本 weaselx64.dll。

**Q: 钉钉 / 飞书 / 微信仍偶发卡顿？**
A: 检查 `%TEMP%\rime.weasel\weasel.crash.log`。若该文件快速增长，说明仍存在未被节流的真实异常（节流只针对 0xE06D7363）。提交 issue 并附 crash log。

**Q: 安装时报"文件被占用"？**
A: 安装器已通过 Restart Manager 自动关闭持有 DLL 句柄的进程。如仍失败，请先手动退出钉钉 / 微信 / 飞书 / Cursor。

**Q: 配色不生效？**
A: 在快速设置栏 → 风格中实时切换预览。或编辑 `weasel.yaml` 的 `style/color_scheme` 字段（区分大小写）。

**Q: 双拼方案如何切换？**
A: `F4` / `Ctrl+Shift+F4` 唤出方案菜单，选择"小鹤双拼"等。

**Q: 如何回滚到原版 rime/weasel？**
A: 在 <https://github.com/rime/weasel/releases> 下载官方安装器，覆盖安装即可。本仓库修改遵循 GPL-3.0，完全兼容。

**Q: 本仓库与上游 rime/weasel 是何关系？**
A: 上游 0.17.4（2025-06-04 发布）是技术基线，本仓库在其上叠加了可视化设置体系与稳定性修订。本仓库的版本号（如 0.18.100）是独立的迭代号，与上游版本号无直接对应关系。可独立使用，也可以无损回滚到上游。

---

## 贡献

欢迎 issue 与 PR。建议流程：

1. Fork 本仓库
2. 创建分支 `git checkout -b feature/your-feature`
3. 提交前先跑 `scripts\build-0.18.100.bat` 验证编译通过
4. PR 描述清楚：功能改进点、使用场景、回归测试

---

## 鸣谢

- [rime/weasel](https://github.com/rime/weasel) — 上游项目
- [rime/librime](https://github.com/rime/librime) — 引擎
- [佛振 / Lotus](https://github.com/lotem) — 创始与维护
- [Mirtle](https://github.com/mirtlecn) — weasel.yaml 整理
- [fxliang](https://github.com/fxliang)、[居戎氏](https://github.com/jurongshijia) 等社区贡献者
- 所有 RIME 用户社区的支持

## 许可

本仓库继承上游 **GPL-3.0** 许可。详见 [LICENSE](LICENSE) 文件。
