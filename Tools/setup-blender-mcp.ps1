<#
  setup-blender-mcp.ps1 - wire Blender MCP into the jam project for Claude Code.

  1. Checks uvx is on PATH; if not, rewrites .mcp.json to use the absolute uvx path (or tells you to install uv).
  2. Copies your existing blender-skills pack (from the Codex install) into <Project>\.claude\skills\ so
     Claude Code gets the same Blender guidance Codex has. Searches common locations; pass -SkillsDir to override.

  Usage:  .\setup-blender-mcp.ps1 -ProjectDir D:\Jam\LakeFight [-SkillsDir <path to blender-skills>]
  Reminder: only ONE MCP client can hold Blender (port 9876) at a time.
#>
param(
  [Parameter(Mandatory=$true)][string]$ProjectDir,
  [string]$SkillsDir
)
$ErrorActionPreference = 'Stop'
$ProjectDir = (Resolve-Path $ProjectDir).Path
$mcp = Join-Path $ProjectDir '.mcp.json'
if (-not (Test-Path $mcp)) { throw "No .mcp.json in $ProjectDir. Run init-jam-repo.ps1 first." }

# 1. uvx
$uvx = Get-Command uvx -ErrorAction SilentlyContinue
if ($uvx) {
  Write-Host "uvx OK: $($uvx.Source)"
} else {
  $cands = @("$env:USERPROFILE\.local\bin\uvx.exe", "$env:USERPROFILE\.cargo\bin\uvx.exe", "$env:LOCALAPPDATA\Programs\uv\uvx.exe")
  $abs = $cands | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $abs) { throw "uvx not found. Install uv:  winget install astral-sh.uv   then re-run." }
  $json = Get-Content $mcp -Raw | ConvertFrom-Json
  $json.mcpServers.blender.command = $abs
  [IO.File]::WriteAllText($mcp, ($json | ConvertTo-Json -Depth 10), (New-Object Text.UTF8Encoding($false)))
  Write-Host "uvx not on PATH; .mcp.json now uses $abs"
}

# 2. blender-skills
if (-not $SkillsDir) {
  $roots = @("$env:USERPROFILE\.codex", "$env:USERPROFILE\.agents", "$env:USERPROFILE\.claude", "$env:USERPROFILE\Documents")
  foreach ($r in $roots) {
    if (-not (Test-Path $r)) { continue }
    $hit = Get-ChildItem $r -Directory -Recurse -Depth 4 -Filter 'blender-skills*' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { $SkillsDir = $hit.FullName; break }
  }
}
if ($SkillsDir -and (Test-Path $SkillsDir)) {
  $dest = Join-Path $ProjectDir '.claude\skills'
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  # copy each folder that contains a SKILL.md
  $skills = Get-ChildItem $SkillsDir -Recurse -Filter 'SKILL.md' | ForEach-Object { $_.Directory }
  foreach ($s in $skills) { Copy-Item $s.FullName (Join-Path $dest $s.Name) -Recurse -Force; Write-Host "skill: $($s.Name)" }
  Write-Host "Vendored $($skills.Count) Blender skill(s) from $SkillsDir. Declare the pack in Docs/ASSETS.md (AI tools)."
} else {
  Write-Host "blender-skills pack not found. Pass -SkillsDir <path>, or continue without it."
}
Write-Host "`nNext: open Blender, start the BlenderMCP add-on server (port 9876), then start Claude Code in $ProjectDir."
