# P - Package and smoke-test (run at least once a day)

1. Run `Tools/ue.ps1 -Action ship -Project <uproject> -Config Development`.
2. If it fails, read `Saved/Logs/package.log`, find the first real error (not warnings), fix only that, retry once.
3. Report: pass/fail, time taken, zip size, and the first error if any.
The human launches the build and plays 2 minutes; do not claim the build "works" from the log alone.
