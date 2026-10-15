# Ticket 05: smoke-validation

**Model:** sonnet

**Acceptance Criteria:**
- [ ] Analysis script `.work/sessions/2026-10-03__export-mesoscopic-species-concentrations/analysis/check_meso_spatial.py` (h5py + numpy), force-added to git. For a 1-event dump it checks that for every snapshot and species, the counts summed over cells equal the `SpeciesMeso.csv` value at the same time (relative tolerance 1e-9). It also checks that `cellSize_nm` never decreases over time and that every position lies inside the box. It prints `OK` or the mismatches.
- [ ] It passes on a Serial run (`/run/beamOn 1`) and on an MT run (`--threads 2`, `/run/beamOn 2`; for MT only the structure and monotonic cell size are checked, since `SpeciesMeso` is a mean over events).
- [ ] A run with the switch off (`beam.in`-like scratch macro with `endTime 1 ms`) creates no `.h5` file and no `.pending_meso_spatial` folder.
- [ ] The file size per event at 10 keV is reported in the commit log.

**Files to Touch:**
- `.work/sessions/2026-10-03__export-mesoscopic-species-concentrations/analysis/check_meso_spatial.py`

**Verification Step:**

Run (after a 1-event Serial run into `.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t05`):
```bash
python .work/sessions/2026-10-03__export-mesoscopic-species-concentrations/analysis/check_meso_spatial.py .scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t05
```

Expected:
`OK`

**Notes:**

- Commit the script with `git add -f` (the project rule: analysis scripts behind reported numbers live in the session's `analysis/` folder).
- `SpeciesMeso.csv` holds the mean count per event, so a 1-event run makes it equal to the summed counts.
- If a mismatch appears, find the cause with systematic debugging before changing code.
