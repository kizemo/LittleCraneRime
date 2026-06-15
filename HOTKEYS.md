# 完整快捷键速查表 · Hotkey Reference

> 适用于 LittleCraneRime（小鹤输入法）0.18.100
>
> 配置文件：
> - 用户配置：`%APPDATA%\Rime\weasel.yaml`
> - 引擎配置：`%APPDATA%\Rime\default.yaml`（patch 段）
> - 模板：`output\data\weasel.yaml`、`output\data\default.yaml`

---

## 1. 中英文切换（核心）

| 操作 | 快捷键 | 实现层 | 备注 |
|------|--------|--------|------|
| **中↔英切换** | **Shift 单按松开** | TSF `OnShiftKeyUpAlone` | **本仓库重点维护项**（0.18.78 起，0.18.100 强化） |
| 中↔英切换 | `Caps Lock` | TSF | 兼容老用户 |
| 中↔英切换 | `Ctrl+Space` | RIME（patch） | 默认开启 |

> ⚠️ **重要**：Shift 单按触发需在 `weasel.yaml` 设置 `customization.toggle_ascii_l: Shift_L`、`customization.toggle_ascii_r: Shift_R`。在 `default.yaml` 中通过 `ascii_composer/switch_key/Shift_L: noop` 禁用 RIME 内同名切换（**避免冲突**）。

### Shift 切换触发逻辑（0.18.78/0.18.100）

```
按下 Shift   → g_shift_armed = true
组合键检测    → Shift+字母/Ctrl+Alt+... = g_shift_combo = true
松开 Shift   → if (combo == false && !letter_seen) _ToggleAsciiMode(TRUE)
            → 进入 lock_rime 窗口（1.5s）
后续 1.5s 内 → 任何 RIME 返回的 ascii_mode 翻回都会被 force-sync 修正
```

---

## 2. 输入方案切换

| 操作 | 快捷键 | 备注 |
|------|--------|------|
| 唤出方案菜单 | `F4` 或 `Ctrl+Shift+F4` | 默认（rime_ice schema） |
| 唤出方案菜单 | `Alt+grave`（`Alt+~`） | 上游默认 |
| 直接选方案 | 数字键 `1-9` | 方案菜单中按数字 |
| 上一/下一方案 | `Shift+Tab` / `Tab` | 菜单内翻动 |

> **内置双拼方案**：本仓库内置了小鹤双拼（flypy）、微软双拼（mspy）、搜狗双拼（sogou）、智能 ABC、加加、紫光、自然码等 7 套双拼方案，可直接通过方案菜单切换。

## 3. 中英标点 / 全半角

| 操作 | 快捷键 | 备注 |
|------|--------|------|
| 中↔英标点 | `Ctrl+.` | rime default.yaml |
| 半↔全角 | `Shift+Space` | rime default.yaml |
| 切换到全角标点 | `Ctrl+Shift+.` | 部分方案支持 |
| 切换到半角标点 | `Ctrl+Shift+Space` | 部分方案支持 |

## 4. 简繁体切换

| 操作 | 快捷键 | 备注 |
|------|--------|------|
| 简↔繁 | `Ctrl+Shift+F3` 或 `F3` | rime_ice 提供 |
| 简↔繁 | `Ctrl+Alt+F` | 部分方案支持 |

## 5. 编辑键

| 操作 | 快捷键 |
|------|--------|
| 退格 | `BackSpace` |
| 删除当前输入 | `Esc` |
| 清空 | `Ctrl+A`（部分方案支持） |
| 翻到首/末候选 | `Home` / `End` |
| 上一/下一候选 | `Shift+Tab` / `Tab` |
| 上一页/下一页候选 | `-` / `=` 或 `PageUp` / `PageDown` |

## 6. 选词上屏

| 操作 | 快捷键 |
|------|--------|
| 上屏第 1-9 候选 | `1` - `9`（数字键） |
| 上屏当前选中候选 | `Space` |
| 上屏当前输入编码（直接上屏） | `Enter`（部分方案） |
| **快速选第二/三候** | `Alt+数字`（rime_ice） |
| 提示词/补全上屏 | `Tab`（部分方案） |

## 7. 高级操作

| 操作 | 快捷键 | 备注 |
|------|--------|------|
| 反查（编码查询） | `Ctrl+grave`（`Ctrl+~`） | 需方案支持 |
| 拆字 | `Ctrl+Alt+E` | 部分方案支持 |
| 调出帮助 | `F1` | 方案相关 |
| **临时英文** | 输入英文后按 `Enter` | 自动转英文上屏 |
| **回车直接上屏** | `Enter` | rime_ice 默认开启 |

## 8. 系统级命令行快捷键

```bat
:: 通过命令行切换（系统级）
WeaselSetup.exe /toggleime          # 注册/取消系统切换键
WeaselSetup.exe /toggleascii        # 注册/取消系统 ascii 切换键

:: 进程内切换
WeaselServer.exe /ascii             # 切到 ASCII（英文）
WeaselServer.exe /nascii            # 切到非 ASCII（中文）
WeaselServer.exe /deploy            # 触发一次重新部署（同步用户配置）
WeaselServer.exe /sync              # 同步
WeaselServer.exe /restart           # 重启服务
WeaselServer.exe /quit              # 退出服务
WeaselServer.exe /help              # 显示帮助
```

## 9. 本仓库新增的快捷键

> 以下是相对上游 rime/weasel **新增**的快捷键，主要服务于"快速设置栏"和"常用短语"等可视化功能。

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| **调出快速设置栏** | **`Alt + ,`** | 可视化设置入口；也可通过托盘区"词典"按钮进入 |
| **调出常用短语面板** | **`Alt + .`** | 鼠标双击或方向键 + 回车上屏 |

上述新增快捷键，均可在快速设置栏 → 快捷键设置中可视化重新绑定，无需手动编辑 `default.yaml`。

## 10. 自定义快捷键

### 9.1 TSF 层（直接处理，不进 RIME）

编辑 `%APPDATA%\Rime\weasel.yaml`：

```yaml
customization:
  # Shift 单按切换中英（默认值，无需改）
  toggle_ascii_l: Shift_L
  toggle_ascii_r: Shift_R
```

### 9.2 RIME 引擎层（按键在 RIME 中处理）

编辑 `%APPDATA%\Rime\default.yaml`（patch 段会 merge 到 rime_ice 等）：

```yaml
patch:
  key_binder/bindings/+:
    - { when: composing, send: Shift+Right, accept: Tab }    # Tab 编码→第二码
    - { when: composing, accept: Alt+period, send: Escape }   # Alt+. 取消
    - { when: always, toggle: ascii_mode, accept: Control+space }
    # 添加更多自定义绑定
```

`when` 条件说明：
- `always` — 任何时候
- `composing` — 组字中（已输入编码）
- `has_menu` — 候选菜单打开
- `paging` — 候选超过一页

## 10. 应用专属快捷键（`weasel.yaml` `app_options`）

```yaml
app_options:
  # 命令行工具强制英文模式
  cmd.exe: { ascii_mode: true }
  powershell.exe: { ascii_mode: true }
  pwsh.exe: { ascii_mode: true }
  windowsterminal.exe: { ascii_mode: true }
  wt.exe: { ascii_mode: true }
  conhost.exe: { ascii_mode: true }
  mintty.exe: { ascii_mode: true }

  # Vim 模式
  nvim-qt.exe: { ascii_mode: true, vim_mode: true }

  # Firefox 行内预编辑
  firefox.exe: { inline_preedit: true }
```

> `vim_mode: true` 后，Vim 编辑器中按 `Esc` / `Ctrl+[` / `Ctrl+c` 会自动切回英文模式。

---

## 故障排查

### Shift 切换中文不生效
1. 确认 `weasel.yaml` 中有 `toggle_ascii_l: Shift_L` / `toggle_ascii_r: Shift_R`
2. 确认 `default.yaml` 中有 `ascii_composer/switch_key/Shift_L: noop`（避免双触发）
3. 重启所有应用（Explorer、钉钉等）让新版 weaselx64.dll 加载
4. 检查 `%TEMP%\rime.weasel\weasel.crash.log` 看有无异常

### Ctrl+Space 切换不灵
1. 可能与 Windows 自身的输入法切换键冲突
2. 在 `default.yaml` 检查 `key_binder` 段
3. 尝试改键为 `Ctrl+grave` 或自定义

### 标点切换错乱
1. 部分方案（如 rime_ice）有自己的标点表
2. 检查方案的 `punctuator` 段
