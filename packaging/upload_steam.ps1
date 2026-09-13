param(
    [Parameter(Mandatory = $true)][string]$ContentDir,
    [Parameter(Mandatory = $true)][int]$AppId,
    [Parameter(Mandatory = $true)][int]$DepotId,
    [string]$SteamCmd = "steamcmd",
    [string]$SteamUser = $env:STEAM_USER
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ContentDir)) {
    Write-Error "ContentDir not found: $ContentDir"
    exit 1
}
if (-not $SteamUser) {
    Write-Error "Set STEAM_USER environment variable or pass -SteamUser"
    exit 1
}

$root = $PSScriptRoot
$out = Join-Path $root "steam_output"
$content = Join-Path $root "steam_content"
New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path $content | Out-Null

Copy-Item -Path (Join-Path $ContentDir "*") -Destination $content -Recurse -Force

$contentEsc = $content.Replace("\", "\\")

$depotVdf = @"
"DepotBuildConfig"
{
  "DepotID" "$DepotId"
  "contentroot" "$contentEsc"
  "FileMapping"
  {
    "LocalPath" "*"
    "DepotPath" "."
    "recursive" "1"
  }
  "FileExclusion" "*.pdb"
  "FileExclusion" "steam_appid.txt"
}
"@
Set-Content -LiteralPath (Join-Path $root "depot_build.vdf") -Value $depotVdf -Encoding ASCII

$template = Get-Content -LiteralPath (Join-Path $root "app_build.vdf.template") -Raw
$appVdf = $template.Replace("APPID_PLACEHOLDER", "$AppId").Replace("DEPOTID_PLACEHOLDER", "$DepotId")
Set-Content -LiteralPath (Join-Path $root "app_build.vdf") -Value $appVdf -Encoding ASCII

& $SteamCmd +login $SteamUser +run_app_build (Join-Path $root "app_build.vdf") +quit
