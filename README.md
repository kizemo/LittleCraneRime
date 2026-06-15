# 小鹤输入法 · LittleCraneRime

[![版本 0.18.100](https://img.shields.io/badge/version-0.18.100-blue.svg)](https://github.com/kizemo/LittleCraneRime/releases)
[![平台](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64%2Bx86-blue.svg)]()
[![基于](https://img.shields.io/badge/fork-rime%2Fweasel-green.svg)](https://github.com/rime/weasel)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-orange.svg)](LICENSE)

> 基于 [rime/weasel](https://github.com/rime/weasel) 的 Windows 小狼毫输入法分叉版本。
> 重点优化：Shift 切换中英文可靠性、TSF 多宿主 C++ 异常防御、钉钉/飞书/微信/Cursor 场景稳定性、安装器自我修复。

---

## 项目简介

**LittleCraneRime（小鹤输入法）** 是对 Rime 输入法 Windows 端 `weasel` 的深度定制分叉。本仓库的目标是**将 Rime 在 Windows 平台的使用体验做到工业级稳定**——尤其是那些在大型应用（钉钉、飞书、微信、Cursor、Office）上会触发的隐藏崩溃与状态异常。

### 为什么分叉？

原版 `rime/weasel` 是非常优秀的项目，但在以下方面存在长年累积的隐藏问题：

| 问题 | 现象 | 影响 |
|------|------|------|
| Shift 单按切换中英文"反弹" | 切到英文后第一个键又自动回中文 | 输入法时灵时不灵 |
| 大量 TSF 宿主下 C++ 异常未防御 | 钉钉/飞书/微信连续输入卡顿 | 用户感知"卡死" |
| 异常后 `weasel.crash.log` 持续膨胀到 GB 级 | 磁盘满 / 性能拖慢 | 静默故障 |
| 安装器偶发"文件被占用" | 升级时 DLL/IME 替换失败 | 升级失败 |
| x64/x86 二进制覆盖 | 打包时 x64 被 x86 覆盖 | 安装包错乱 |

本仓库通过 **12 个版本（0.18.52–0.18.100）** 的迭代验证，把上述问题逐项修复。

---

## 版本与上游

- **基础版本**：rime/weasel `0.18.99`（基于 rime/librime 1.13.1）
- **当前版本**：0.18.100
- **分叉作者**：[kizemo](https://github.com/kizemo)
- **上游来源**：<https://github.com/rime/weasel>
- **许可协议**：GPL-3.0（与上游一致）

---

## 核心特性（相对上游的修改）

### 1. 安全性增强

#### 1.1 多层崩溃诊断与节流（0.18.86–0.18.100）
- **VEH（Vectored Exception Handler）记录**：捕获 `weaselx64.dll` 内的所有 0xE06D7363 (C++ 异常) 与 0x80000003 (断点)，写日志
- **线程局部节流**：每线程每 1000ms 最多 1 次
- **全局节流**：进程内每 500ms 最多 1 次
- **真实崩溃永不丢失**：access violation 等未知异常不受节流影响
- **结果**：crash log 不再无限膨胀，连续输入 1 小时 < 1MB（实测 92MB → 0.5MB）

#### 1.2 EditSession 异常防御（0.18.86）
- 所有 edit session（`CStartCompositionEditSession`、`CEndCompositionEditSession` 等）经 `RunEditSessionSafe` 包装
- C++ 异常转 SEH、SEH 转 HRESULT，最终不会让宿主进程崩溃
- 钉钉/微信/Cursor/飞书 上的 `0xE06D7363` 不再传播

#### 1.3 TSF 回调异常防御（0.18.91）
- `OnKeyDown` / `OnTestKeyDown` / `OnKillThreadFocus` / `OnSetThreadFocus` 等回调经 `RunTSFCallback` 包装
- 异常被吞，返回 E_FAIL，TSF 继续工作

### 2. Shift 切换中英文可靠性

#### 2.1 修复 Shift 单独释放切换后立即"反弹"（0.18.97、0.18.100）
**Bug**：用户按 Shift 单独松开 → 切换到英文模式 → 第一个键又被 RIME 引擎翻回中文。
**根因**：原 lock-rime 逻辑在第一次 key 确认匹配时立即清空锁，1.5s 内的第二次 key 若被 RIME 翻回，lock 已失效。
**修复**：
- `Composition.cpp::WeaselTSF::_AfterRimeKey` — 新增 `within_lock_window` 判定
- 锁窗口内（1.5s）保留 lock，持续 force-sync
- 锁窗口外且模式匹配时再释放 lock
- `EditSession.cpp` 同步应用相同逻辑

#### 2.2 Shift + 组合键防误触（0.18.78）
- `ShiftComboGuard` 模块在 Shift 单独释放时才触发 toggle
- Shift+字母、Shift+Ctrl+字母等组合场景**不进入** `_ToggleAsciiMode`
- 详见 `include/ShiftComboGuard.h`

### 3. 安装器自我修复

#### 3.1 文件占用强制关闭（0.18.97、0.18.100）
- `taskkill /f /im /t` 杀 Weasel 全家桶 + 子进程
- **Windows Restart Manager API**：枚举持有 `weaselx64.dll` 句柄的进程，强制关闭或显示提示
- `Delete /REBOOTOK` 覆盖所有 DLL/IME/exe，包括 `7z.exe`、`curl.exe`、WinSparkle

#### 3.2 x64/x86 二进制不互相覆盖（0.18.99）
- NSIS 打包时使用 `/oname=output\Win32\<filename>` 显式指定子目录
- 修复了 `WeaselServer.vcxproj` 缺少 trailing-backslash 导致 x64 产物被覆盖的 bug
- 安装包内 x64 与 x86 共存，互不干扰

#### 3.3 配置目录注册表保护（0.18.97）
- `WeaselSetup.exe /upgrade` 时检测 RimeUserDir 路径，必要时合并到注册表
- 避免升级后用户配置丢失

### 4. 键事件管线重构（R-B1, 0.18.89）

重构前 vs 重构后：

```
旧管线: Test(仅测键) → KeyDown(两次 AfterRimeKey) → KeyUp(再次重复)
新管线: Test(仅测键) → KeyDown(唯一 AfterRimeKey) → KeyUp(跳过已处理)
```

- `KeySinkState` 单一结构体，状态机化
- 移除 async coalesce（导致 Bug）
- 新增 `_HandleIdleKeyResponse` / `_RequestKeyEditSession`
- 连续输入 + 暂停 ≥ 2min 首键 + Cursor/微信交替，均稳定

### 5. 候选框与字体修复

- **C-D13 风格灌入**：候选框不可见（font=0）→ 修复
- **高亮背景未绘制**：fullscreen 布局下高亮错误 → 修复
- **GPU reset 后绘制失败**：使用 DirectWrite Resources 兜底

### 6. 微信 / 飞书 / 钉钉专项

| 应用 | 修复 | 版本 |
|------|------|------|
| 微信（Weixin.exe） | 连续输入 0xE06D7363 防御 | 0.18.86 |
| 飞书（Feishu.exe） | TSF 多宿主异常防御 | 0.18.91 |
| 钉钉（DingTalk.exe） | Shift 切换 + 异常防御 | 0.18.97 |
| Cursor | TSF 兼容 + 异常防御 | 0.18.91 |
| Firefox | 行内预编辑修复（issue #946） | 内置 |

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

### 2. 快捷键（默认行为）

> **重要**：以下快捷键在 TSF 层（`WeaselTSF::OnKeyUp/OnKeyDown`）直接处理，**不经过 RIME 引擎**。

#### 2.1 中英文切换

| 行为 | 快捷键 | 实现层 | 说明 |
|------|--------|--------|------|
| **中/英切换** | **Shift 单按松开** | TSF（`OnShiftKeyUpAlone`） | **0.18.78 验证稳定**；松手才触发，按住不触发 |
| 中/英切换 | `Caps Lock` | TSF（已弃用，请改用 Shift） | 老式方案 |
| 中/英切换 | `Control+Space` | RIME（patch 添加到 default.yaml） | 传统方案 |

#### 2.2 模式切换（方案选择）

| 行为 | 快捷键 | 来源 |
|------|--------|------|
| 切换输入方案 | `Control+Shift+F4` / `F4` | rime default.yaml |
| 切换简化/繁体 | `Control+Shift+F3` / `F3` | 方案相关 |
| 切换半角/全角 | `Shift+Space` | rime default.yaml |
| 切换中英标点 | `Control+.` | rime default.yaml |

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

#### 2.4 系统级快捷键（命令行设置）

```bat
# 设置全局系统快捷键
WeaselSetup.exe /toggleime                # 启用/禁用输入法切换
WeaselSetup.exe /toggleascii              # 切换 ASCII 模式
WeaselServer.exe /ascii                   # 命令行切到 ASCII
WeaselServer.exe /nascii                  # 命令行切到非 ASCII
```

### 3. RIME 引擎快捷键（`default.yaml`）

```yaml
# 默认 patch（在 patch 块下，merge 进 rime_ice 或 rime 默认 schema）
patch:
  key_binder/bindings/+:
    - { when: composing, send: Shift+Right, accept: Tab }    # 编码→第二码
    - { when: composing, accept: Alt+period, send: Escape }   # 取消组字
    - { when: always, toggle: ascii_mode, accept: Control+space }  # Ctrl+Space 切换

  ascii_composer/switch_key/Shift_L: noop   # 关键：禁用 RIME 内 Shift 切换（由 TSF 处理）
  ascii_composer/switch_key/Shift_R: noop

  switcher:
    hotkeys: [Control+Shift+F4, F4, Alt+comma]
    # 其他
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

### 验证安装

```powershell
# 验证进程
Get-Process WeaselServer
Get-Process WeaselDeployer

# 验证 DLL 加载
Get-Process -Name excel -ErrorAction SilentlyContinue | 
  Select-Object -First 1 | 
  ForEach-Object { $_.Modules | Where-Object ModuleName -like 'weasel*' }

# 验证版本
(Get-Item "C:\Program Files\Rime\weasel-*\weaselx64.dll").VersionInfo.FileVersion
```

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
├── WeaselIPC/             # 命名管道 + 请求/响应协议
├── WeaselIPCServer/       # Server 端 IPC 处理
├── WeaselUI/              # DirectUI 候选框渲染
├── WeaselDeployer/        # 部署/同步工具
├── WeaselSetup/           # 安装器
├── RimeWithWeasel/        # librime 适配层
├── include/               # 公共头
│   ├── WeaselCrashDiag.h     # VEH 节流（本仓库新增）
│   ├── WeaselEditSessionSafe.h
│   ├── WeaselTSFCallbackSafe.h
│   ├── ShiftComboGuard.h     # Shift 防误触
│   └── ...
├── scripts/               # 构建/打包/部署脚本
├── docs/                  # 设计文档
│   ├── FIX_ATTEMPTS.md       # 0.18.52-0.18.100 试错全记录
│   └── PROJECT_REVIEW.md     # 0.18.100 改造清单
├── output/                # 编译产物
│   ├── data/                 # 用户配置 data/
│   ├── weasel.dll            # x86
│   ├── weaselx64.dll         # x64
│   ├── WeaselServer.exe
│   ├── WeaselSetup.exe
│   └── install.nsi
└── weasel.props           # 版本号 / 输出路径
```

---

## 已知差异（与上游 rime/weasel 0.18.99）

| 模块 | 上游 | 本仓库 | 原因 |
|------|------|--------|------|
| `WeaselTSF::OnKeyUp` 逻辑 | 重复 AfterRimeKey | 单次调用（`g_key_eaten` 守卫） | R-B1 重构 |
| `_PullResponseAfterKey` 状态机 | 简单读 IPC | 加 `idle` / `compose` 分支 | 0.18.87 |
| VEH Crash Log | 不限流 | 1000ms 线程级 + 500ms 全局节流 | 0.18.100 |
| Shift lock-rime 行为 | 1 次确认即释放 | 1.5s 窗口内持续 force-sync | 0.18.100 |
| `appcast.xml` 标题 | 自动维护 | 同上 + 手动 patch | 流程差异 |
| NSIS `/oname=` | 默认单层 | 显式分 x64/x86 子目录 | 0.18.99 |

详细差异见 `docs/PROJECT_REVIEW.md` 与 git log。

---

## 文档

- [CHANGELOG.md](CHANGELOG.md) — 版本变更记录
- [HOTKEYS.md](HOTKEYS.md) — 完整快捷键速查表
- [docs/FIX_ATTEMPTS.md](docs/FIX_ATTEMPTS.md) — 0.18.52 → 0.18.100 试错全记录（50+ 个假设验证）
- [docs/PROJECT_REVIEW.md](docs/PROJECT_REVIEW.md) — 0.18.100 改造清单与架构图
- [docs/SHIFT_COMBO_ANALYSIS.md](docs/SHIFT_COMBO_ANALYSIS.md) — Shift 切换的失效场景分析
- [output/data/weasel.yaml](output/data/weasel.yaml) — 默认 `weasel.yaml` 模板
- [output/data/default.yaml](output/data/default.yaml) — RIME 引擎默认配置

---

## 常见问题

### Q: 升级后 Shift 切换又失效？
A: 升级后请重启所有应用。新安装器更新磁盘上的 `weaselx64.dll`，但已在内存中加载的进程（Explorer、钉钉等）仍使用旧版本。重启进程即可。

### Q: 钉钉/飞书仍然卡顿？
A: 检查 `%TEMP%\rime.weasel\weasel.crash.log`。如果该文件快速增长，说明仍存在未被节流的真实异常（节流只针对 0xE06D7363）。提交 issue 并附上 crash log。

### Q: 安装时报"文件被占用"？
A: 安装器会通过 Restart Manager 自动关闭持有 DLL 句柄的进程。如果仍然失败，请先手动退出钉钉/微信/飞书/Cursor。

### Q: 配色不生效？
A: 编辑 `%APPDATA%\Rime\weasel.yaml`，确认 `style/color_scheme` 字段拼写正确（区分大小写）。可在"输入法设定 → 外观"中实时切换预览。

### Q: 如何回滚到原版 rime/weasel？
A: 在 <https://github.com/rime/weasel/releases> 下载官方安装器，覆盖安装即可。本仓库修改 GPL-3.0，完全兼容。

---

## 贡献

欢迎 issue 与 PR！建议流程：

1. Fork 本仓库
2. 创建分支 `git checkout -b fix/your-issue`
3. 提交前先跑 `scripts\build-0.18.100.bat` 验证编译通过
4. PR 描述清楚：bug 复现步骤、修复思路、回归测试
5. 涉及崩溃的修复请附 `%TEMP%\rime.weasel\weasel.crash.log`

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
