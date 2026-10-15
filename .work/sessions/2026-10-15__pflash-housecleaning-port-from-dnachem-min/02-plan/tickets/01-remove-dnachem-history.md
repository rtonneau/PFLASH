# Ticket 01: remove-dnachem-history

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 1)
**Effort:** low

**Acceptance Criteria:**
- [ ] All sessions except the current one are removed with `clean.js`.
- [ ] `.work/retro`, `.work/reads`, `.work/scratch`, `.work/changelog`, `.claude/plans/*`, `.claude/plan-class-based-chemistry.md`, `.kb-notes/`, `.claude/hooks/hook-posttooluse.log`, `report` and `docs/cloud-work/` are gone.
- [ ] `projectNotesPath` is removed from `.claude/.claude-project.json`; no remaining file points to `.kb-notes`, `.claude/plans/` contents or `docs/cloud-work`.
- [ ] `.work/GLOSSARY.md`, `.work/gps-config.json` and the current session still exist.

**Files to Touch:**
- `.work/sessions/*` (old ones), `.work/retro/`, `.work/reads/`, `.work/scratch/`, `.work/changelog/`
- `.claude/plans/`, `.claude/plan-class-based-chemistry.md`, `.kb-notes/`, `report`, `docs/cloud-work/`
- `.claude/.claude-project.json`, `CLAUDE.md` (only dangling links)

**Verification Step:**

Run:
```bash
git status --short | head; ls .work/sessions; grep -rIn "kb-notes\|cloud-work\|projectNotesPath" --exclude-dir=.git --exclude-dir=.work . | head
```

Expected:
Only the current session under `.work/sessions`; the grep prints nothing apart from kept history files (ADRs, CHANGELOG, old review).

**Notes:**

List sessions with `status.js`, then delete with `clean.js <id...>` and confirm; never touch `2026-10-15__pflash-housecleaning-port-from-dnachem-min`. The correct plan file name is `.claude/plan-class-based-chemistry.md`. Use `git rm -r` for the plain files.
