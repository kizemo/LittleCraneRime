# Post-install verification for Weasel TSF deployment.
param(
    [string]$ExpectedVersion = "0.18.53",
)

$ErrorActionPreference = 'Stop'
$failures = @()

function Fail([string]$msg) {
    $script:failures += $msg
    Write-Host "FAIL: $msg" -ForegroundColor Red
}

function Pass([string]$msg) {
    Write-Host "PASS: $msg" -ForegroundColor Green
}

$weaselRoot = $null
foreach ($regPath in @(
    'HKLM:\SOFTWARE\Rime\Weasel',
    'HKLM:\SOFTWARE\WOW6432Node\Rime\Weasel'
)) {
    $prop = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
    if ($prop -and $prop.WeaselRoot) {
        $weaselRoot = $prop.WeaselRoot
        break
    }
}
if (-not $weaselRoot) {
    Fail "HKLM\SOFTWARE\Rime\Weasel\WeaselRoot is empty (checked 64-bit and WOW6432Node views)"
} else {
    Pass "WeaselRoot=$weaselRoot"
}

$src = Join-Path $weaselRoot 'weaselx64.dll'
$sys = Join-Path $env:WINDIR 'System32\weasel.dll'
if (-not (Test-Path $src)) { Fail "Missing $src" }
if (-not (Test-Path $sys)) { Fail "Missing $sys" }
if ((Test-Path $src) -and (Test-Path $sys)) {
    $srcLen = (Get-Item $src).Length
    $sysLen = (Get-Item $sys).Length
    if ($srcLen -ne $sysLen) {
        Fail "System32 size ($sysLen) != install size ($srcLen)"
    } else {
        Pass "System32 weasel.dll size matches install dir ($sysLen bytes)"
    }
    $sysVer = (Get-Item $sys).VersionInfo.FileVersion
    if ($sysVer -notlike "$ExpectedVersion*") {
        Fail "System32 version=$sysVer expected $ExpectedVersion*"
    } else {
        Pass "System32 version=$sysVer"
    }
}

$tsfLog = Join-Path $env:TEMP 'rime.weasel\weasel.tsf.log'
if (-not (Test-Path $tsfLog)) {
    Fail "Missing $tsfLog"
} else {
    $lines = Get-Content $tsfLog -Encoding Unicode -ErrorAction SilentlyContinue
    if (-not $lines) { $lines = Get-Content $tsfLog -ErrorAction SilentlyContinue }
    $loaded = $lines | Where-Object { $_ -match "weasel\.dll loaded $ExpectedVersion" } | Select-Object -Last 1
    $active = $lines | Where-Object { $_ -match "WeaselTSF $ExpectedVersion ActivateEx" } | Select-Object -Last 1
    if (-not $loaded) { Fail "weasel.tsf.log lacks weasel.dll loaded $ExpectedVersion" }
    else { Pass $loaded }
    if (-not $active) { Fail "weasel.tsf.log lacks WeaselTSF $ExpectedVersion ActivateEx" }
    else { Pass $active }
    $stale = $lines | Where-Object { $_ -match 'WeaselTSF 0\.18\.(39|40|41) ActivateEx' } | Select-Object -Last 3
    if ($stale) {
        Write-Host "WARN: older ActivateEx lines still in log (restart WeChat/apps if testing):" -ForegroundColor Yellow
        $stale | ForEach-Object { Write-Host "  $_" }
    }
}

$installLog = Join-Path $env:TEMP 'rime.weasel\install.log'
if (Test-Path $installLog) {
    $tail = Get-Content $installLog -Tail 8 -ErrorAction SilentlyContinue
    $tail | ForEach-Object { Write-Host "install.log: $_" }
}

if ($failures.Count -gt 0) {
    Write-Host "`nVerification FAILED ($($failures.Count) issue(s))." -ForegroundColor Red
    exit 1
}

Write-Host "`nVerification PASSED. Restart WeChat before input testing." -ForegroundColor Green
exit 0
