# Ticket 06: final-verification

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 0.99)
**Effort:** low

**Acceptance Criteria:**
- [ ] No stale `dnachem-min`, `sim.exe`, `sim.cc`, `sim-output`, `.kb-notes` outside the kept history and the origin note.
- [ ] Fresh `build/` builds `pflash`; `build-ninja/` ctest passes.
- [ ] Smoke run: 10 keV e-, `/run/beamOn 2`, `/scheduler/endTime 1 ms`, dump, writes `Species.Txt`, `Manifest.json`, `TrackLengths.csv`.

**Files to Touch:**
- none expected (fixes only if the checks fail)

**Verification Step:**

Run:
```bash
grep -rIn "dnachem-min\|sim\.exe\|sim\.cc\|sim-output" --exclude-dir=.git --exclude-dir=.work . | grep -v "docs/adr\|CHANGELOG\|docs/reviews\|docs/literature-comparison" | head
```
then build, ctest and the smoke run per `.claude/geant4-instructions.md`.

Expected:
Only origin-note lines; build and tests succeed; the smoke output files exist.

**Notes:**

Report honestly if anything fails. Scratch macro goes in the run build dir (untracked), per `.claude/.claude-project.json` smoke notes.
