# 宿主闪退诊断指南

## 日志位置

| 文件 | 用途 |
|------|------|
| `%TEMP%\rime.weasel\weasel.crash.log` | **崩溃专档**：`report` 一行摘要 + `crumb` 面包屑轨迹 |
| `%TEMP%\rime.weasel\weasel.tsf.log` | TSF 全量；崩溃时另有 `crash` / `crash-detail` |
| `%TEMP%\rime.weasel\*.dmp` | UEF 迷你转储（`宿主-weasel.dll-时间.pid.dmp`） |

## 崩溃报告格式（`[report]`）

```
host=Cursor.exe ver=0.18.72 tid=... code=0xE06D7363 addr=... depth=... composing=... ipc=... sess=...
```

| 字段 | 含义 |
|------|------|
| `host` | 崩溃宿主进程名 |
| `code` | 异常码（`0xE06D7363` = MSVC C++ 异常） |
| `depth` | EditSession 嵌套深度 |
| `composing` | 是否组字中 |
| `ipc` | IPC 是否已连接 |
| `sess` | WeaselServer 会话 ID |

## 面包屑（`[crumb]`）

崩溃前最多 **40** 条关键路径标记，例如：

- `lifecycle`：ActivateEx / Deactivate
- `edit-session`：DoEditSession 进入/退出
- `ipc`：Reconnect / Connect failed
- `ui`：UpdateUI

**根据最后几条 crumb 定分支**，避免盲目改码。详见 `FIX_ATTEMPTS.md` 路线图。

## 排查流程

1. 打开 `weasel.crash.log` 最新 `report`
2. 向上读 `crumb` 序列，对照时间线
3. 若有 `*.dmp`，保留文件名与时间
4. 更新 `FIX_ATTEMPTS.md` 证伪/新分支

## 常见模式（当前已知）

| crumb 模式 | 可能分支 |
|------------|----------|
| `Deactivate` → `crash` | C-D4 服务退出/Deactivate 路径 |
| `Reconnect Connect failed` 连发 → `crash` | IPC 断连后仍访问会话 |
| `edit-session` 刷屏 → `crash` | C-D3 EditSession 重入（已部分修复） |
| 无 crumb、仅 `ActivateEx` → `crash` | 初始化/语言栏/候选窗 |

## 禁止

- 无 `crash-detail` / `crumb` 时猜测根因并大改
- 重复 `FIX_ATTEMPTS.md` 证伪表中的分支
