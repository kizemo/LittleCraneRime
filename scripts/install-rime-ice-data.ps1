param(
  [string]$Proxy = "http://127.0.0.1:7897",
  [string]$Tag = "nightly",
  [string]$DataDir = "",
  [switch]$UsePlum
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $DataDir) {
  $DataDir = Join-Path $root "output\data"
}

function Ensure-Dir([string]$Path) {
  if (-not (Test-Path $Path)) {
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
  }
}

function Clear-PresetData([string]$TargetDir) {
  $keepPreview = Join-Path $TargetDir "preview"
  Get-ChildItem $TargetDir -Force -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.Name -eq "preview") { return }
    if ($_.PSIsContainer) {
      Remove-Item $_.FullName -Recurse -Force
    } else {
      Remove-Item $_.FullName -Force
    }
  }
  Ensure-Dir $keepPreview
}

function Install-FromZip([string]$TargetDir) {
  $zipUrl = "https://github.com/iDvel/rime-ice/releases/download/$Tag/full.zip"
  $zipPath = Join-Path $root "rime-ice-full.zip"
  $tempDir = Join-Path $root "rime-ice-full-tmp"

  Write-Host "Downloading rime-ice full.zip ($Tag) ..."
  $curlArgs = @("--ssl-no-revoke", "-L", "--retry", "5", "-o", $zipPath, $zipUrl)
  if ($Proxy) { $curlArgs = @("-x", $Proxy) + $curlArgs }
  & curl.exe @curlArgs
  if (-not (Test-Path $zipPath) -or (Get-Item $zipPath).Length -lt 1000000) {
    throw "Failed to download rime-ice full.zip"
  }

  if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
  Ensure-Dir $tempDir
  Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force

  # full.zip unpacks flat (yaml at archive root); do not pick a subfolder like cn_dicts/
  $sourceDir = Get-Item $tempDir
  if (-not (Test-Path (Join-Path $sourceDir.FullName "default.yaml"))) {
    $nested = Get-ChildItem $tempDir -Directory | Where-Object {
      Test-Path (Join-Path $_.FullName "default.yaml")
    } | Select-Object -First 1
    if ($nested) { $sourceDir = $nested }
  }

  Ensure-Dir $TargetDir
  Clear-PresetData $TargetDir
  Copy-Item -Path (Join-Path $sourceDir.FullName "*") -Destination $TargetDir -Recurse -Force
  Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host "Installed rime-ice into $TargetDir"
}

function Install-FromPlum([string]$TargetDir) {
  $plumDir = Join-Path $root "plum"
  if (-not (Test-Path (Join-Path $plumDir "rime-install"))) {
    throw "plum submodule missing. Run: git submodule update --init plum"
  }
  Ensure-Dir $TargetDir
  Clear-PresetData $TargetDir
  $env:plum_dir = $plumDir
  $env:rime_dir = $TargetDir
  $recipe = "iDvel/rime-ice:others/recipes/full"
  Write-Host "Installing via plum recipe: $recipe"
  & bash (Join-Path $plumDir "rime-install") $recipe
  if ($LASTEXITCODE -ne 0) { throw "plum rime-install failed with exit code $LASTEXITCODE" }
}

Ensure-Dir $DataDir
if ($UsePlum) {
  Install-FromPlum $DataDir
} else {
  Install-FromZip $DataDir
}

& (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "patch-rime-ice-config.ps1") -DataDir $DataDir
Write-Host "Done. Default schemes: rime_ice + double_pinyin_sogou."
