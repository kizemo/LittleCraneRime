# 小鹅 (Weasel) 调试与发布历史

## 版本里程碑

### 0.18.90 / 0.18.92 / 0.18.93 — Rime 循环依赖修复 + TSF 回调日志
- **问题**：weasel.dll 在 WeChat、TRAE SOLO、Cursor、Directory Opus 中崩溃
- **根因**：`rime_ice.schema.yaml` 通过 `dependencies` 引用 `melt_eng` 和 `radical_pinyin`，这两个 schema 又通过 `import_preset: default` 间接引用回 rime_ice，形成循环依赖
- **修复**：删除 `dependencies` 字段，将 `radical_reverse_lookup.dictionary` 改为 `melt_eng`
- **0.18.93**：在 `WeaselTSF.cpp` 嵌入 TSF 框架回调日志（默认关闭，env var `WEASEL_TSF_CB_LOG`=1 开启）
- **结果**：崩溃仍然存在（日志显示 `code=0xE06D7363`、地址固定 `0x00007FFB84EA1B6A`）

### 0.18.94 — SEH 屏障（C+D 方案）
- **方案**：在 `EditSession::DoEditSession` 外层添加 SEH 屏障
- **实现**：使用 `_set_se_translator` 将 SEH 异常转换为 C++ 异常，再用 try-catch 捕获
- **vcxproj 修改**：为 `EditSession.cpp` 添加 `/EHa` 编译选项
- **结果**：SEH 屏障成功编译，但用户反馈崩溃仍然存在

### 0.18.95 — 自动关闭占用进程（首次落地完整的 process_closer）
- **背景**：用户使用 Directory Opus 作为默认文件管理器，安装时如果不手动关闭，weasel.dll 被占用导致安装失败；且"之前的分析是正确的，安装流程存在问题，没有更新应该更新的的文件"
- **新增模块**：
  - `WeaselSetup/process_closer.{h,cpp}` 使用 Windows Restart Manager API
  - 三级关闭流水线：WM_QUERYENDSESSION → WM_CLOSE → TerminateProcess
  - 关键系统进程白名单（explorer.exe、csrss.exe 等）
- **集成点**：修改 `imesetup.cpp::copy_file`，遇到 `ERROR_SHARING_VIOLATION` 时自动调用
- **打包验证**：
  - 0.18.95 installer SHA256 = `8941E0611686183DFF07C8A733A90AEE15FF6C6650EE547D8615CA22184DACE2`
  - 0.18.95 WeaselSetup.exe = 397824 字节（0.18.94 = 387072 字节，+10K）

## 待解决的 Bug（0.18.96+）

### Bug #1 — shift 切换英文的 UI 同步失效
- **现象**：按 shift 切换英文时，托盘区图标变成英文图标，但实际键盘输入还是中文
- **可能根因**：ascii_mode 状态变量更新后，托盘图标与 IME 内部状态不同步

### Bug #2 — 三键组合快捷键错误切换英文
- **现象**：shift+ctrl+F 或 shift+ctrl+K 等三键组合快捷键，在实现正确快捷键功能的同时，会错误切换英文（ascii mode）
- **可能根因**：hotkey 处理时没有过滤 shift 状态，导致 shift 被识别为 ascii_mode 切换

### Bug #3 — 微信/TRAE SOLO 崩溃仍未解决
- **现象**：SEH 屏障（0.18.94）实施后，仍然崩溃
- **可能根因**：
  - SEH 屏障只捕获 weaselx64.dll 内部的异常，但崩溃点在 dll 边界外
  - 崩溃可能由其他 dll（如 msctf.dll、TextInputFramework.dll）触发
  - 需要更深入的崩溃日志

## 重要教训

1. **打包验证**：每次打包前必须验证 installer 内嵌的文件时间戳和大小，确认新版本确实被打包进去（否则用户运行旧代码）
2. **process_closer**：已实现为通用机制，所有后续版本都会自动处理 dll 占用
3. **微信/TRAE SOLO 特殊**：两者崩溃频繁，需要特别排查 TSF 框架回调路径

## 下一步

- 排查 Bug #1、#2、#3 的根因
- 在 0.18.96 中一并修复
- 打包前验证新 WeaselSetup.exe、weaselx64.dll 时间戳与大小
- 启用 process_closer 自动关闭所有占用进程
