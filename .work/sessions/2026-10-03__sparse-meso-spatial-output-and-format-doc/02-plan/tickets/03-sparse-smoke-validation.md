# Ticket 03: sparse-smoke-validation

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `.work/sessions/2026-10-03__sparse-meso-spatial-output-and-format-doc/analysis/check_meso_spatial.py`, adapted from `.work/sessions/2026-10-03__export-mesoscopic-species-concentrations/analysis/check_meso_spatial.py`, additionally checks `formatVersion == 2`, that `formatDoc` exists, and that no `counts` row sums to 0.
- [ ] On a Serial 1-event run (10 keV, endTime 1 ms) it prints `OK`, including the exact summed-count match with `SpeciesMeso.csv` at all times.
- [ ] The size per event is reported and compared with v1 (about 28 MB per event).

**Files to Touch:**
- `.work/sessions/2026-10-03__sparse-meso-spatial-output-and-format-doc/analysis/check_meso_spatial.py`

**Verification Step:**

Run:
```bash
C:/Users/rtonneau/miniconda3/envs/GEANT4_py311/python.exe .work/sessions/2026-10-03__sparse-meso-spatial-output-and-format-doc/analysis/check_meso_spatial.py .scratch/tests/2026-10-03__sparse-meso-spatial-output-and-format-doc/t03
```

Expected:
`OK`

**Notes:**

The script lives in `.work/`, so it is committed by this session with `git add -f` and recorded with `ticket-complete.js --commit`. Run `sim` with the macro first and flags after (`./sim t03.in --dir ...`).
