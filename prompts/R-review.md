# R - Review a finished task (use after every CurrentTask)

Review the last task's work against `Docs/CurrentTask.md`.
1. `git diff --stat` and the full diff of gameplay code. List every file touched outside the task's scope.
2. Build the editor target. Report the exact result.
3. Check every acceptance item: say PASS / FAIL / NEEDS HUMAN, with evidence (log line, file, value).
4. Check AGENTS.md jam rules: anything copied from another project? any unlogged third-party asset?
5. Do not fix anything. Output a short list of problems, most important first.
