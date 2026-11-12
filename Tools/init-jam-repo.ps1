<#
  init-jam-repo.ps1 - run AFTER the jam opens (10:30 IST / 05:00 UTC, 12 Nov 2026),
  once you have created the new Unreal project in the editor.

  What it does:
    1. Refuses to run before the jam start time (keeps the git history clean for Epic).
    2. Copies agent files and Docs templates from the kit into the project.
    3. git init + Git LFS + first commit "Jam start".

  Usage:
    .\init-jam-repo.ps1 -ProjectDir D:\Jam\LineDown
#>
param(
  [Parameter(Mandatory=$true)][string]$ProjectDir,
  [switch]$Force   # skip the start-time check (only for testing on a scratch folder)
)
$ErrorActionPreference = 'Stop'
$jamStartUtc = [datetime]::SpecifyKind([datetime]'2026-11-12T05:00:00', 'Utc')
if (-not $Force -and [datetime]::UtcNow -lt $jamStartUtc) {
  throw "The jam has not started yet (opens 05:00 UTC / 10:30 IST). Wait, then run again."
}

$ProjectDir = (Resolve-Path $ProjectDir).Path
$uproject = Get-ChildItem $ProjectDir -Filter '*.uproject' | Select-Object -First 1
if (-not $uproject) { throw "No .uproject in $ProjectDir. Create the project in the editor first." }
$name = $uproject.BaseName
$kit = Split-Path $PSScriptRoot
$tpl = Join-Path $kit 'templates'

# Copy templates, never overwriting existing files
Get-ChildItem $tpl -Recurse -File -Force | ForEach-Object {
  $rel = $_.FullName.Substring($tpl.Length + 1)
  $dest = Join-Path $ProjectDir $rel
  if (Test-Path $dest) { Write-Host "skip (exists) $rel"; return }
  New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
  $text = [IO.File]::ReadAllText($_.FullName).Replace('{{PROJECT}}', $name)
  [IO.File]::WriteAllText($dest, $text, (New-Object Text.UTF8Encoding($false)))   # no BOM (JSON/TOML safe)
  Write-Host "added $rel"
}

# Tools live with the project so agents can call them
$toolsDest = Join-Path $ProjectDir 'Tools'
New-Item -ItemType Directory -Force -Path $toolsDest | Out-Null
foreach ($t in 'ue.ps1','import_fbx.py','setup-blender-mcp.ps1') { Copy-Item (Join-Path $PSScriptRoot $t) $toolsDest -Force }

Push-Location $ProjectDir
$ErrorActionPreference = 'Continue'   # git prints progress on stderr
try {
  if (-not (Test-Path '.git')) { git init -b main | Out-Null }
  git lfs install --local | Out-Null
  git add -A
  git commit -m "Jam start: empty $name project + declared pre-jam tooling" | Out-Null
  git log --oneline -1
  Write-Host "`nRepo ready. First commit time (UTC): $([datetime]::UtcNow.ToString('u'))"
} finally { Pop-Location }
