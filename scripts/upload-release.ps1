param(
  [string]$ReleaseDir = "",
  [string]$RemoteBase = "https://www.aiec.fun/pinyin"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $ReleaseDir) {
  $ReleaseDir = Join-Path $root "release"
}

$installer = Get-ChildItem -Path $ReleaseDir -Filter "weasel-*-installer.exe" |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
$appcast = Join-Path $ReleaseDir "appcast.xml"

if (-not $installer) {
  Write-Error "No installer found in $ReleaseDir"
}
if (-not (Test-Path $appcast)) {
  Write-Error "appcast.xml not found in $ReleaseDir"
}

$length = $installer.Length
$appcastContent = Get-Content $appcast -Raw -Encoding UTF8
$appcastContent = [regex]::Replace($appcastContent, 'length="[0-9]+"', "length=`"$length`"", 1)
[System.IO.File]::WriteAllText($appcast, $appcastContent, [System.Text.UTF8Encoding]::new($false))

Write-Host "Installer: $($installer.FullName) ($length bytes)"
Write-Host "Appcast:   $appcast"
Write-Host ""
$notes = Join-Path $ReleaseDir "release-notes.html"
Write-Host "Upload these files to $RemoteBase/ :"
Write-Host "  - $($installer.Name)"
Write-Host "  - appcast.xml"
if (Test-Path $notes) { Write-Host "  - release-notes.html" }

# Try SCP upload if UPLOAD_HOST env is set
$uploadHost = $env:UPLOAD_HOST
$uploadPath = $env:UPLOAD_PATH
if ($uploadHost -and $uploadPath) {
  $remote = "${uploadHost}:${uploadPath}/"
  scp $installer.FullName $remote
  scp $appcast $remote
  Write-Host "Uploaded to $remote"
} else {
  Write-Host "Set UPLOAD_HOST and UPLOAD_PATH env vars for automatic SCP upload."
}
