# Ticket 02: species-time-grid

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] `ScoreSpecies.cc`'s constructor calls `AddTimeToRecord` with log-spaced points (1-2-5 sequence) from 1 ps to 999999 ps: 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 999999 (all `* CLHEP::picosecond`), replacing the current 7-entry list. The first (1 ps) and last (999999 ps) values are unchanged from today.
- [ ] No other line in `ScoreSpecies.cc` changes.
- [ ] A smoke run's `Species.Txt` shows 19 distinct recorded times instead of the current 7.

**Files to Touch:**
- `src/scoring/ScoreSpecies.cc`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
cd build
echo -e "/gun/particle e-\n/gun/energy 25 keV\n/run/initialize\n/scheduler/endTime 1 ms\n/run/beamOn 10\n/run/dumpDataAndReset" > macro/scratch_timegrid_smoke.in
./sim.exe scratch_timegrid_smoke.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket02-smoke > ticket02-smoke.log 2>&1
```

Expected: exit 0, no `EEEE`/`FatalException`; the distinct time values in `Species.Txt` (or `Species_nt_species.csv`) number 19, with the smallest 1 ps and the largest 999999 ps (same bounds as before this ticket).

**Notes:**

This is a project-wide, shared grid (every Chemistry's `Species.Txt`/`.csv` output gets finer), not something scoped to the new `Farokhi2023` Chemistry — keep the change to exactly the `AddTimeToRecord` call list, nothing else in the file.
