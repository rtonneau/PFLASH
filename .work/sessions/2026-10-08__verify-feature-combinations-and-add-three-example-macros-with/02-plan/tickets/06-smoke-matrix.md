# Ticket 06: smoke-matrix

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.57)
**Effort:** medium

**Acceptance Criteria:**
- [ ] 3 modes x {O2, no O2} x {Serial, `--threads 4`} (12 runs, 10 keV, 2 events, end time at most 1 us) complete without crash or unexpected warning.
- [ ] `Species.*`, `Reactions.*` exist for every run; `SpeciesMeso.*` only with meso on; manifests show the right mode; with O2, e_aq decays faster than without.
- [ ] Any interference found is fixed with a test where practical, or recorded as an open issue in the session log.

**Files to Touch:**
- `.scratch/tests/` (run artifacts, not committed)
- source files only if a defect is found

**Verification Step:**

Run:
```bash
cd build && ls results_* | head -50
```

Expected:
The expected file set per run, as listed in the criteria.

**Notes:**

Use a scripted loop in the scratch dir. Background runs: see `.claude/geant4-instructions.md`. Report a table of the 12 runs in the commit log.
