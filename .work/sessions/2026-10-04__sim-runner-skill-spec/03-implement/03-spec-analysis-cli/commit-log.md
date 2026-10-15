# Ticket 03: spec-analysis-cli

**Status:** ✅ Done

## Local Test Result

Subcommand grep loop -> `ok` for enqueue, status, show, log, cancel, requeue, prune, analyze, analyses, dispatcher; `grep -c TBD` 0; headings Analysis, Subcommands, Installation, Build milestones present. PASS.

## Review Notes

Drafted by a sonnet subagent; reviewed against the criteria. Fixed after review: a blank line splitting the Global settings table (left by ticket 02). Subagent defaults: exit codes 5 (no match) / 6 (analysis failed), `--runs` is a file `<out>/runs.json` (schemaVersion 1), succeeded runs only unless `--all-states`, generic `run_stats.py` / `manifest_table.py`, launcher `g4run.py` + `g4runlib/`, psutil>=7,<8, `fake_sim.c` test double, `analysisTimeoutMin` 120. Committed in dotfiles on `docs/g4run-spec` (SPEC.md only); pointer doc updated here.

## Blockers / Challenges

None. Spec notes that bootstrap.ps1's mklink fallback lacks /D for directory links (for the build session).

## Commits

- 22d06e7 docs: g4run spec analysis and CLI sections in dotfiles (ticket 03)

## Time Spent

5m (ticket-start.js to ticket-complete.js)
