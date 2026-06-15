param(
  [string]$ExpectedVersion = "0.18.53",
  [string]$AppcastUrl = "https://www.aiec.fun/pinyin/appcast.xml"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$localAppcast = Join-Path $root "release\appcast.xml"

Write-Host "=== Local appcast ==="
if (-not (Test-Path $localAppcast)) {
  Write-Host "FAIL: missing $localAppcast" -ForegroundColor Red
  exit 1
}
$text = Get-Content $localAppcast -Raw -Encoding UTF8
if ($text -notmatch '(?s)<item>.*?sparkle:version="([^"]+)".*?weasel-([0-9.]+)-installer\.exe') {
  Write-Host "FAIL: cannot parse first appcast item" -ForegroundColor Red
  exit 1
}
$ver = $Matches[1]
$urlVer = $Matches[2]
Write-Host "Latest sparkle:version=$ver installer=$urlVer"
if ($ver -ne $ExpectedVersion) {
  Write-Host "FAIL: first item version is $ver, expected $ExpectedVersion" -ForegroundColor Red
  exit 1
}
$items = [regex]::Matches($text, 'weasel-[0-9.]+-installer\.exe')
if ($items.Count -ge 2 -and $items[0].Value -eq $items[1].Value) {
  Write-Host "FAIL: first two items share the same installer URL" -ForegroundColor Red
  exit 1
}
Write-Host "PASS: local appcast structure OK" -ForegroundColor Green

Write-Host "`n=== Remote appcast (optional) ==="
try {
  $remote = Invoke-WebRequest -Uri $AppcastUrl -UseBasicParsing -TimeoutSec 15
  if ($remote.Content -match 'sparkle:version="([^"]+)"') {
    $rver = $Matches[1]
    Write-Host "Remote latest version=$rver"
    if ([version]$rver -ge [version]$ExpectedVersion) {
      Write-Host "PASS: remote feed reachable" -ForegroundColor Green
    } else {
      Write-Host "WARN: remote version $rver < $ExpectedVersion (upload pending?)" -ForegroundColor Yellow
    }
  }
} catch {
  Write-Host "WARN: cannot fetch $AppcastUrl - $($_.Exception.Message)" -ForegroundColor Yellow
}

exit 0
