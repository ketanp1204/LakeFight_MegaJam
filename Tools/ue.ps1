<#
  ue.ps1 - Unreal command-line wrapper for the MegaJam (generic tooling, prepared pre-jam).

  Actions:
    detect    List installed UE versions, show which one will be used.
    build     Compile the <Project>Editor target (Development, Win64). Close the editor first if you
              changed headers; Live Coding otherwise holds the DLLs.
    package   BuildCookRun Shipping Win64 -> <ProjectDir>\Build\Windows. Logs to Saved\Logs\package.log
    run       Launch the latest packaged .exe.
    zip       Zip the packaged build to <ProjectDir>\Build\<ProjectName>_<yyyyMMdd_HHmm>.zip for itch.io.
    ship      package + zip.

  Examples:
    .\ue.ps1 -Action detect
    .\ue.ps1 -Action package -Project D:\Jam\LineDown\LineDown.uproject
    .\ue.ps1 -Action ship -Project D:\Jam\LineDown\LineDown.uproject -Config Development
#>
param(
  [Parameter(Mandatory=$true)][ValidateSet('detect','build','package','run','zip','ship')][string]$Action,
  [string]$Project,
  [string]$EngineDir,
  [ValidateSet('Shipping','Development')][string]$Config = 'Shipping'
)
$ErrorActionPreference = 'Stop'

function Find-Engine {
  if ($EngineDir) { return (Resolve-Path $EngineDir).Path }
  if ($env:UE_ROOT) { return $env:UE_ROOT }
  $roots = @("$env:ProgramFiles\Epic Games", 'C:\Program Files\Epic Games', 'D:\Epic Games', 'D:\Program Files\Epic Games', 'E:\Epic Games')
  $found = foreach ($r in $roots | Select-Object -Unique) {
    if (Test-Path $r) { Get-ChildItem $r -Directory -Filter 'UE_5.*' -ErrorAction SilentlyContinue }
  }
  if (-not $found) { throw "No UE_5.* install found. Pass -EngineDir 'C:\Program Files\Epic Games\UE_5.8' or set UE_ROOT." }
  # highest version wins
  $best = $found | Sort-Object { [version]($_.Name -replace 'UE_','') } -Descending | Select-Object -First 1
  return $best.FullName
}

function Require-Project {
  if (-not $Project) { throw "-Project <path to .uproject> is required for '$Action'." }
  if (-not (Test-Path $Project)) { throw "Project not found: $Project" }
  return (Resolve-Path $Project).Path
}

$engine = Find-Engine
$uat = Join-Path $engine 'Engine\Build\BatchFiles\RunUAT.bat'
$ubt = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'

switch ($Action) {
  'detect' {
    Write-Host "Using engine: $engine"
    if (Test-Path (Join-Path $engine 'Engine\Plugins\Experimental\ModelContextProtocol')) {
      Write-Host "Epic Unreal MCP plugin folder found (ModelContextProtocol)."
    } else {
      Write-Host "Epic Unreal MCP plugin folder NOT found under Engine\Plugins\Experimental. Check the Plugin Browser for 'Unreal MCP' (UE 5.8+)."
    }
    foreach ($t in @($uat, $ubt)) { Write-Host ("{0,-6} {1}" -f ($(if (Test-Path $t) {'OK'} else {'MISSING'})), $t) }
    if (Get-Command git-lfs -ErrorAction SilentlyContinue) { Write-Host 'git-lfs OK' } else { Write-Host 'git-lfs MISSING (run: winget install GitHub.GitLFS)' }
  }
  'build' {
    $p = Require-Project; $name = [IO.Path]::GetFileNameWithoutExtension($p)
    & $ubt "${name}Editor" Win64 Development "-Project=$p" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)" }
  }
  { $_ -in 'package','ship' } {
    $p = Require-Project; $dir = Split-Path $p
    $out = Join-Path $dir 'Build'
    $log = Join-Path $dir 'Saved\Logs\package.log'
    New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $ErrorActionPreference = 'Continue'   # UAT writes to stderr; don't let PS 5.1 treat that as fatal
    & $uat BuildCookRun "-project=$p" -noP4 -platform=Win64 "-clientconfig=$Config" -build -cook -allmaps -stage -pak -archive "-archivedirectory=$out" -prereqs -nodebuginfo -utf8output 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { throw "Package failed ($code). See $log" }
    Write-Host ("Packaged in {0:mm\:ss} -> {1}" -f $sw.Elapsed, $out)
    if ($Action -eq 'package') { break }
    $Action = 'zip'
  }
}

if ($Action -eq 'zip') {
  $p = Require-Project; $dir = Split-Path $p; $name = [IO.Path]::GetFileNameWithoutExtension($p)
  $win = Get-ChildItem (Join-Path $dir 'Build') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'Windows*' } | Select-Object -First 1
  if (-not $win) { throw "No packaged build in $dir\Build. Run -Action package first." }
  $zip = Join-Path $dir ("Build\{0}_{1}.zip" -f $name, (Get-Date -Format 'yyyyMMdd_HHmm'))
  Compress-Archive -Path (Join-Path $win.FullName '*') -DestinationPath $zip -Force
  Write-Host ("Zip: {0} ({1:N0} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
}
elseif ($Action -eq 'run') {
  $p = Require-Project; $dir = Split-Path $p
  $exe = Get-ChildItem (Join-Path $dir 'Build') -Recurse -Filter '*.exe' -ErrorAction SilentlyContinue |
         Where-Object { $_.DirectoryName -notmatch '\\Engine\\' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if (-not $exe) { throw "No packaged .exe found. Run -Action package first." }
  Write-Host "Launching $($exe.FullName)"; Start-Process $exe.FullName
}
