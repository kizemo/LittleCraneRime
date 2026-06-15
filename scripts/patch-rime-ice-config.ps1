param(
  [string]$DataDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $DataDir) {
  $DataDir = Join-Path $root "output\data"
}

$defaultYaml = Join-Path $DataDir "default.yaml"
if (-not (Test-Path $defaultYaml)) {
  Write-Warning "default.yaml not found, skip patch."
  exit 0
}

$lines = [System.IO.File]::ReadAllLines($defaultYaml, [System.Text.UTF8Encoding]::new($false))
$out = New-Object System.Collections.Generic.List[string]
$i = 0
while ($i -lt $lines.Count) {
  if ($lines[$i] -match '^schema_list:\s*$') {
    $out.Add('schema_list:')
    $out.Add('  - schema: rime_ice')
    $out.Add('  - schema: double_pinyin_sogou')
    $out.Add('')
    $i++
    while ($i -lt $lines.Count -and $lines[$i] -match '^[ \t]') { $i++ }
    continue
  }
  $out.Add($lines[$i])
  $i++
}
$content = ($out -join "`r`n") + "`r`n"

# Shift 单击不切换中英文（commit_code 在 Shift+标点组合后仍会误切换）
# 中英文切换改由 key_binder 的 Control+space
if ($content -notmatch 'toggle: ascii_mode, accept: Control\+space') {
  $content = $content -replace '(?m)(    - \{ when: has_menu, accept: Shift_R, send: 3 \}\r?\n)',
    "`$1`r`n    - { when: always, toggle: ascii_mode, accept: Control+space }`r`n"
}
# 主翻译器启用用户词典（组词选词自动学习）
if ($content -notmatch 'enable_user_dict:\s*true') {
  $content = $content -replace '(translator:\s*\r?\n(?:[^\r\n]+\r?\n)*?)(  dictionary: rime_ice)',
    "`$1  enable_user_dict: true`r`n`$2"
}

# 启用逗号/句号翻页，注释掉减号/等号翻页避免冲突
$content = $content -replace '(?m)^    - \{ when: has_menu, accept: minus, send: Page_Up \}', '    # - { when: has_menu, accept: minus, send: Page_Up }'
$content = $content -replace '(?m)^    - \{ when: has_menu, accept: equal, send: Page_Down \}', '    # - { when: has_menu, accept: equal, send: Page_Down }'
$content = $content -replace '(?m)^    # - \{ when: has_menu, accept: comma, send: Page_Up \}', '    - { when: has_menu, accept: comma, send: Page_Up }'
$content = $content -replace '(?m)^    # - \{ when: has_menu, accept: period, send: Page_Down \}', '    - { when: has_menu, accept: period, send: Page_Down }'

# 移除双击 Shift 翻页绑定（与 Shift 切换中英文冲突）
$content = $content -replace '(?m)^    - \{ when: has_menu, accept: Shift\+Shift_L, send: 2 \}\r?\n', ''
$content = $content -replace '(?m)^    - \{ when: has_menu, accept: Shift\+Shift_R, send: 3 \}\r?\n', ''

# 移除 key_binder 中 Shift 切换中英文（组合键会误触发，改由 ascii_composer commit_code）
$content = $content -replace '(?m)^    - \{ when: always, toggle: ascii_mode, accept: Shift_L \}\r?\n', ''
$content = $content -replace '(?m)^    - \{ when: always, toggle: ascii_mode, accept: Shift_R \}\r?\n', ''

# Shift 单击切换改由 TSF 层处理，ascii_composer 保持 noop 避免 Shift+标点误切
$content = $content -replace 'Shift_L: commit_code', 'Shift_L: noop'
$content = $content -replace 'Shift_R: commit_code', 'Shift_R: noop'

# 中文模式下 / 输出顿号
$content = $content -replace "(?m)^    '/' : '/'", "    '/' : '、'"

$content = [regex]::Replace(
  $content,
  "config_version: '[^']*'",
  "config_version: '2026-06-11-aiec-v4'"
)

[System.IO.File]::WriteAllText($defaultYaml, $content, [System.Text.UTF8Encoding]::new($false))
Write-Host "Patched $defaultYaml"

Get-ChildItem -Path $DataDir -Filter "*.schema.yaml" | ForEach-Object {
  $schemaText = [System.IO.File]::ReadAllText($_.FullName, [System.Text.UTF8Encoding]::new($false))
  if ($schemaText -match 'db_class:\s*stabledb') {
    $schemaText = $schemaText -replace 'db_class:\s*stabledb', 'db_class: tabledb'
    [System.IO.File]::WriteAllText($_.FullName, $schemaText, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Patched $($_.Name) custom_phrase.db_class -> tabledb"
  }
}
