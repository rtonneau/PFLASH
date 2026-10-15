# Session: Convert GValues discussion into an issue

**Date:** 2026-09-25T14:12:37.542Z
**Status:** Grill phase complete

## Problem Statement

G-values in `Species_nt_species.csv` (`sumG`) are inflated by a factor (nEvents+1)/2 within a single `/run/beamOn N`. `ActionInitialization.cc:71` sets `G4MoleculeCounterManager::SetResetCountersBeforeEvent(false)`, but `ScoreSpecies::EndOfEvent` reads the molecule counter after every event and divides by that event's own `fEdep` (`ScoreSpecies.cc:145`). Event k therefore reads the cumulative molecule count of events 1..k, so the summed G grows with the number of events per run.

Reproduction (Serial, 10 keV e-, `/scheduler/endTime 1 ns`, `/run/beamOn N`, mean G = `sumG/nEvent` for e_aq^-1 at 1 ps):

| events | e_aq G | factor vs 1 event |
|---|---|---|
| 1 | 3.84 | 1 |
| 4 | 9.75 | 2.5 |
| 8 | 17.54 | 4.5 |

Older runs in `.scratch` fit the same pattern: 6 events give 14.05 and 10 events give 22.0, i.e. about 4.0 x (n+1)/2. Independent cross-check (50 keV x 4 events): 10264 `G4DNAIonisation` and 10128 `G4DNAElectronSolvation` steps, versus 20274 H3O+ and 19964 e_aq at 1 ps.

## Context & Constraints

- Not a units problem: `n_mol / (fEdep/eV) * 100` is dimensionally correct (`fEdep` in MeV internal units, `eV` = 1e-6 MeV), and `sumG/nEvent` equals N/E*100 exactly for the checked runs.
- `sumG` is a sum over events (mean G = `sumG/nEvent`) and the x100 is already applied, so a raw N/E in molecules/eV is 100x smaller than the per-100 eV value.
- `ScoreSpecies` reads the counter per event (chem6 style). Geant4 examples chem4, chem5, chem6, scavenger, dnadamage2 and molcounters/sdcounters use `SetResetCountersBeforeEvent(true)`; only molcounters/basic uses `false` (it reads once per run).
- `SetResetCountersBeforeEvent` is process-wide, so the fix affects Serial and MT alike.
- Ground rule: verify against source; the counter index is per molecule (`G4MoleculeCounterIndex`), so there is no double counting from the index itself.

## Success Metrics

- Mean G (`sumG/nEvent`) is independent of the number of events: 1-event and 8-event runs at the same energy agree within statistical noise (e_aq^-1 at 1 ps about 3.8-4, OH about 4.9).
- MT and Serial give the same mean G.
- `number / (E_total/eV) * 100` equals `sumG/nEvent` for every species and time.

## Architecture & Approach

1. In `src/ActionInitialization.cc` `BuildMoleculeCounters()`, change line 71 to `SetResetCountersBeforeEvent(true)` and update the comment.
2. Verify MT: run with `--threads N` and compare mean G against Serial (the old MT runs showed the same (n+1)/2 pattern per thread).
3. Add a smoke check: a scripted `sim.exe` comparison of mean G for `beamOn 1` vs `beamOn 8` (needs a Geant4 kernel, so not a ctest unit test).
4. Note that existing outputs and `.scratch` results produced with `beamOn N>1` are inflated and must be regenerated.

Bounded work: implement directly on the checked-out branch, no plan document.

## Assumptions & Trade-offs

- Assumes `SetResetCountersBeforeEvent(true)` resets the counter at event start in both Serial and MT the same way chem6 relies on; to be confirmed by the MT check.
- `SetResetCountersBeforeRun(true)` stays as is.
- Alternative (subtract the previous event's cumulative counts in `EndOfEvent`) was rejected: it duplicates what the counter manager already provides and diverges from the Geant4 examples.

## Open Questions

- Does MT need any additional handling (worker counters accumulating into master) once per-event reset is on? Check with an MT run.
- Should the smoke check live in `test/` as a script or stay a documented manual procedure in `.claude/geant4-instructions.md`?

## Notes

- Scratch evidence: `.scratch/gtest/` (e10, e50, n1, n8 outputs and logs).
- Any G-value already computed from multi-event runs, including the `.scratch/variability` runs, is affected.

## Token Usage

- **Input:** 12
- **Output:** 3293
- **Cache read:** 561457
- **Cache creation:** 10006
- **Total:** 574768
