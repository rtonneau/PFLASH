# Session: mt-dump-clear-worker-scorers

**Date:** 2026-09-24T16:06:37.521Z
**Status:** Grill phase complete

## Problem Statement

In MT mode, `/run/dumpDataAndResetToDir` (and `/run/dumpDataAndReset`) write the master's accumulated data, which is correct for reaction counts, physics interactions and energy deposit. Species data is double-counted across `/run/beamOn` calls: `ScoreSpecies::AbsorbResultsFromWorkerScorer` (`src/ScoreSpecies.cc:173-249`) merges the worker scorer's `fSpeciesInfoPerTime` into the master but never clears it on the worker. `OutputAndClear()` returns early on worker threads, and the only other worker-side clear is inside the disabled `#ifdef _ScoreSpecies_FOR_ALL_EVENTS`. Every `beamOn` therefore re-merges the workers' cumulative species map, so `sumG`, `sumG2` and `number` in `Species_nt_species.csv` are inflated relative to `nEvent` (which is reset per run). Example: `beamOn 4`, `dumpDataAndResetToDir run01`, `beamOn 4`, `dumpDataAndResetToDir run02` gives a `run02` that includes `run01`'s contribution.

## Context & Constraints

- Merge path: `Run::Merge` (`src/Run.cc`) calls `masterScorer->AbsorbResultsFromWorkerScorer(localScorer)`. The reaction and interaction counters are already cleared on the worker after merge (`src/Run.cc:93,96`). Energy is per-`Run` (fresh each `beamOn`). The master `Run` has no `TimeStepAction`/`SteppingAction`, so it uses its own fresh counters.
- The dump commands share `RunAccumulatorMessenger::WriteAllAndReset`, so one fix covers both.
- Serial mode has no worker-to-master merge and is unaffected.
- Existing unit tests are kernel-free and cannot exercise `ScoreSpecies` (keyed by `G4MolecularConfiguration*`).
- Project rule: `macro/reactions.in` has pre-existing uncommitted changes unrelated to this work; do not commit it.

## Success Metrics

- After the fix, in MT mode, a second `beamOn` + dump yields species `sumG/nEvent` consistent with a run-2-only reference (per-event means, statistical comparison since RNG seeds differ per run).
- Before the fix, the same macro shows the inflation (bug reproduced).
- Serial mode and the reaction/interaction/energy output files are unchanged.

## Architecture & Approach

Bounded change. In `ScoreSpecies::AbsorbResultsFromWorkerScorer`, add `right->fSpeciesInfoPerTime.clear();` after the merge loop, next to `right->fNEvent = 0;`, mirroring the pattern `Run::Merge` uses for the counters. Update the stale comment in `RunAction.cc` if it implies the worker state persists. `RunAccumulatorMessenger` is untouched; no new API or macro command.

Verification is a scripted MT smoke run (no new committed test code), run from `build/` with a 10 keV electron gun and a throwaway macro under `.scratch/tests/2026-09-24__mt-dump-clear-worker-scorers/`: `sim --threads 2`, `beamOn 4`, `dumpDataAndResetToDir run01`, `beamOn 4`, `dumpDataAndResetToDir run02`, before and after the fix.

## Assumptions & Trade-offs

- Assumes the master thread's `mfDetector` scorer (looked up by `RunAccumulatorMessenger`) is the one `Run::Merge` merges into; the existing design already relies on this.
- Trade-off: no committed regression test, because an MT integration test needs the Geant4 kernel and the Debug test binary has known linking problems with the differently-built Geant4 install. The user chose the scripted smoke run.
- The check is statistical, not exact, because worker RNG seeds differ per run.

## Open Questions

None.

## Notes

Session directory records the finding first raised in chat: reaction/interaction counters and energy are correct; only the species scorer leaks.

## Token Usage

- **Input:** 14
- **Output:** 4068
- **Cache read:** 542211
- **Cache creation:** 11073
- **Total:** 557366
