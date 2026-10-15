# Ticket 02: spec-execution

**Model:** opus

**Acceptance Criteria:**
- [ ] Sections Dispatcher, Admission, Run wrapper, Failures & requeue, Cancel, Prune, Notifications added.
- [ ] Dispatcher: single-instance lock, auto-revive by any subcommand, sleep until slot or next `--at`, exit when queue empty, overdue warning in `status` after reboot.
- [ ] Admission: thread budget (Serial = 1; default physical cores − 1), memory estimate formula from history with fallbacks (project default, `--mem` override), free-RAM margin default 15 %.
- [ ] Run wrapper: PID + create time, peak RSS sampling, stdout.log, exit code, success = exit 0 and a successMarker and no failureMarker; lost-run reconciliation.
- [ ] Cancel (confirm, process-tree kill), requeue (new run linked to original), prune (manual, sizes, confirm, rows marked pruned, `--binaries`), toast mechanism chosen and justified.
- [ ] Committed in dotfiles (SPEC.md only).

**Files to Touch:**
- `~/dotfiles/claude/skills/g4run/SPEC.md`

**Verification Step:**

Run:
```bash
grep -E '^## ' ~/dotfiles/claude/skills/g4run/SPEC.md; grep -c TBD ~/dotfiles/claude/skills/g4run/SPEC.md
```

Expected:
Ticket 1 headings plus Dispatcher, Admission, Run wrapper, Failures & requeue, Cancel, Prune, Notifications; TBD count 0.

**Notes:**

Closes the grill's open questions on free-RAM margin, memory scaling and toast mechanism. sim writes data only at `/run/dumpDataAndReset` or exit, so a killed run has partial output at most.
