# Ticket 01: sbs-reference-capture

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `macro/validate_water.in` and `macro/validate_o2.in` are added. Each contains:
  - no `/process/chem/TimeStepModel` line;
  - PureWater, `/dnaLogger/verbose Error`;
  - a fixed seed (`/random/setSeeds 12345 67890`);
  - `/run/initialize`;
  - `/scheduler/endTime 1 us` after `/run/initialize`;
  - 10 keV e-;
  - `/run/beamOn N`;
  - `/run/dumpDataAndReset`.
  
  The `_o2` macro also has `/chem/env/scavenger O2 21 %`. N is chosen so the relative standard error on G(1 µs) for e_aq, °OH, H2O2 and H2 is ≤ 3%, and a comment states that value.
- [ ] Both macros run under the current SBS build (exit 0, success markers present, no `EEEE`). `--threads 4` is allowed.
- [ ] `validation/reference/sbs_water/` and `validation/reference/sbs_o2/` each contain the run's `Species_nt_species.csv` and `Manifest.json`, plus a `README.md` naming the commit `1d047c4`, the macro, the thread count, N, the wall time and the measured relative error per species.

**Files to Touch:**
- `macro/validate_water.in`, `macro/validate_o2.in`
- `validation/reference/sbs_water/{Species_nt_species.csv,Manifest.json,README.md}`
- `validation/reference/sbs_o2/{Species_nt_species.csv,Manifest.json,README.md}`

**Verification Step:**

Run (PowerShell, MSVC env; trial first, then the sized run in the background):
```bash
cmake --build build --target sim
cd build; ./sim validate_water.in --threads 4 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/01-sbs_water > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/01-sbs_water.log 2>&1
./sim validate_o2.in --threads 4 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/01-sbs_o2 > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/01-sbs_o2.log 2>&1
```

Expected:
Both exit 0. The logs contain `dumped and reset` and `The simulation took`, and no `EEEE`. Each reference `README.md` reports a relative error ≤ 3% for the four species.

**Notes:**

Do not change any source file in this ticket: it records today's behaviour. Size N with a short trial (e.g. 4 events), using the per-event spread of G(1 µs) from that trial. The relative error is the standard error of the mean over events, which needs per-event values. If `Species_nt_species.csv` only has totals, run the trial as several `/run/beamOn` blocks with a dump each, or estimate from repeated small runs, and state the method in the README. The `.gitignore` must not exclude `validation/`.
