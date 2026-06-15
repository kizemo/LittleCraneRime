param(
  [string]$ProductVersion = $env:PRODUCT_VERSION,
  [string]$WeaselVersion = $env:WEASEL_VERSION
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$template = Join-Path $root "update\appcast.xml"
$releaseDir = Join-Path $root "release"
$out = Join-Path $releaseDir "appcast.xml"

if (-not $ProductVersion) { $ProductVersion = "0.17.5.0" }
if (-not $WeaselVersion) { $WeaselVersion = "0.17.5" }
$ProductVersion = $ProductVersion.Trim()
$WeaselVersion = $WeaselVersion.Trim()

$content = [System.IO.File]::ReadAllText($template, [System.Text.UTF8Encoding]::new($false))

# Only update the newest (first) <item>; preserve all historical entries verbatim.
$pattern = '(?s)(?<head>.*?<item>)(?<body>.*?)(?<close></item>)(?<tail>.*)'
if ($content -notmatch $pattern) {
  throw "appcast template missing first <item>"
}
$firstItem = $Matches['body']
$firstItem = [regex]::Replace($firstItem, 'weasel-[0-9.]+-installer\.exe',
  "weasel-$ProductVersion-installer.exe")
$firstItem = [regex]::Replace($firstItem, 'sparkle:version="[0-9.]+"',
  "sparkle:version=`"$WeaselVersion`"")
$firstItem = [regex]::Replace($firstItem, '(<title>小狼毫 )[0-9.]+(</title>)',
  "`${1}$WeaselVersion`${2}")

$installer = Get-ChildItem -Path $releaseDir -Filter "weasel-$ProductVersion-installer.exe" -ErrorAction SilentlyContinue |
  Select-Object -First 1
if (-not $installer) {
  $parentRelease = Join-Path (Split-Path -Parent $root) "release"
  $installer = Get-ChildItem -Path $parentRelease -Filter "weasel-$ProductVersion-installer.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
}
if ($installer) {
  $firstItem = [regex]::Replace($firstItem, 'length="[0-9]+"', "length=`"$($installer.Length)`"")
}

$content = $Matches['head'] + $firstItem + $Matches['close'] + $Matches['tail']

if (-not (Test-Path $releaseDir)) {
  New-Item -ItemType Directory -Path $releaseDir | Out-Null
}
[System.IO.File]::WriteAllText($out, $content, [System.Text.UTF8Encoding]::new($false))
$notesSrc = Join-Path $root "update\release-notes.html"
$notesDst = Join-Path $releaseDir "release-notes.html"
if (Test-Path $notesSrc) {
  Copy-Item -Path $notesSrc -Destination $notesDst -Force
  Write-Host "Copied release-notes.html"
}
Write-Host "Generated $out"
