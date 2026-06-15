$ErrorActionPreference = 'Continue'
$dstDir = 'D:\Program Files\Rime\weasel-0.18.33'
$logOut = 'd:\soft\02office\rime\weasel\accept_test_result.txt'

function Write-Result($msg) {
    Add-Content -Path $logOut -Value $msg
    Write-Host $msg
}

Remove-Item $logOut -ErrorAction SilentlyContinue
Write-Result "=== Weasel 0.18.33 acceptance test (elevated) ==="
Write-Result "Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Result "Admin: $(([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))"

Write-Result ""
Write-Result "=== Step 2: Kill processes ==="
foreach ($p in @('WeaselServer','ctfmon','TextInputHost')) {
    $r = & taskkill /F /IM "$p.exe" 2>&1
    Write-Result "taskkill $p : $r"
}
Start-Sleep -Seconds 2

Write-Result ""
Write-Result "=== Step 3: WeaselSetup.exe /upgrade ==="
Push-Location $dstDir
& ".\WeaselSetup.exe" /upgrade
$exitCode = $LASTEXITCODE
Pop-Location
Write-Result "ERRORLEVEL: $exitCode"

Write-Result ""
Write-Result "=== Step 4: install.log last 10 lines ==="
$logPath = "$env:TEMP\rime.weasel\install.log"
if (Test-Path $logPath) {
    $bytes = [System.IO.File]::ReadAllBytes($logPath)
    $enc = if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        [Text.Encoding]::Unicode
    } else {
        [Text.Encoding]::UTF8
    }
    $content = $enc.GetString($bytes)
    $lines = $content -split "`r?`n" | Where-Object { $_ -match '\S' }
    $lines | Select-Object -Last 10 | ForEach-Object { Write-Result $_ }
} else {
    Write-Result "install.log not found"
}

Write-Result ""
Write-Result "=== Step 5: DLL size comparison ==="
$sys32 = "$env:WINDIR\System32\weasel.dll"
$wow64 = "$env:WINDIR\SysWOW64\weasel.dll"
$instX64 = "$dstDir\weaselx64.dll"
$instX86 = "$dstDir\weasel.dll"
foreach ($item in @(
    @{N='System32\weasel.dll';P=$sys32},
    @{N='SysWOW64\weasel.dll';P=$wow64},
    @{N='install\weaselx64.dll';P=$instX64},
    @{N='install\weasel.dll';P=$instX86}
)) {
    if (Test-Path $item.P) {
        $fi = Get-Item $item.P
        Write-Result ("{0,-26} {1,10} bytes  {2}" -f $item.N, $fi.Length, $fi.LastWriteTime)
    } else {
        Write-Result ("{0,-26} NOT FOUND" -f $item.N)
    }
}

if ((Test-Path $sys32) -and (Test-Path $instX64)) {
    $s32 = (Get-Item $sys32).Length
    $x64 = (Get-Item $instX64).Length
    if ($s32 -eq $x64) {
        Write-Result "PASS: System32\weasel.dll size matches weaselx64.dll ($s32 bytes)"
    } else {
        Write-Result "FAIL: System32\weasel.dll ($s32) != weaselx64.dll ($x64)"
    }
}

$lastLine = $lines | Select-Object -Last 1
if ($lastLine -match 'install\(\) success' -and $exitCode -eq 0) {
    Write-Result "OVERALL: PASS"
} else {
    Write-Result "OVERALL: FAIL (exit=$exitCode, lastLog=$lastLine)"
}
