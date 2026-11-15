if (Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*Docs/N1.md*' -and $_.ProcessId -ne $PID }) { exit }
Set-Location "C:\Unreal\Projects\LakeFight_MegaJam"
$log = "Saved\n1_resume_$(Get-Date -Format yyyyMMdd_HHmm).log"
claude -p "Read Docs/N1.md and follow it as a resumed session." --dangerously-skip-permissions --output-format stream-json --verbose *> $log