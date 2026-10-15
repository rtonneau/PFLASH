# Session: primary-and-secondary-tracklength-scorers

**Date:** 2026-10-10T11:19:33.757Z
**Status:** Grill phase complete

## Problem Statement

The physical stage has no direct record of how far the primary travels or how much path its secondary electrons cover. Add per-event scoring of (1) the **Primary track** length, with its start and end kinetic energy and its end reason, so the LET is ΔE / L without proxies, and (2) the summed track length and count of the **Secondary electrons**, both all generations and first generation (ParentID == 1).

## Context & Constraints

- **Current behavior:** `SteppingAction` counts G4DNA interactions into a `PhysicsInteractionCounter`, which goes through `Run::Merge` / `RunAccumulator` and is written at each Dump. `TrackingAction` is empty. Path length and LET are only estimated as proxies from PreChemical files by `docs/literature-comparison/farokhi2023/track_length_let.py`; those proxies miss elastic and vibrational steps and the secondary KE.
- **Pain point:** the proxies are biased, especially for electrons, which scatter a lot, and they can't tell a stopped primary from one that escaped the box or was killed.
- **Dependencies:** keep the accumulate-across-beamOn / write-at-Dump / reset model (ADR 0005); Serial and MT both work through `Run::Merge`; `PrimaryKiller` (track 1 e- only, eLossMin/minKineticE kill, eLossMax abort) stays as it is apart from the new flag; aborted events are skipped, as the other outputs already do.
- **Tech stack:** C++20, Geant4 11.4.1 (G4EmDNAPhysics), CTest with plain `assert` in `build-ninja/` (Debug), `JsonWriter`/`DataNode` for the manifest.

## Success Metrics

- Each Dump writes `TrackLengths.Txt` and `TrackLengths.csv`, one row per non-aborted event, sorted by (run, event), with columns `run, event, primaryLength_nm, primaryEkin0_keV, primaryEkinEnd_keV, primaryEnd, secondaryFirstGen_nm, nSecondaryFirstGen, secondaryAll_nm, nSecondaryAll`. Both files are listed in `Manifest.json` `files`.
- `Manifest.json` has a `trackLengths` summary: the event count and the mean ± SEM of each numeric column.
- A 10 keV e- smoke run (`/run/beamOn 2`, 1 ms end time) gives `primaryEnd = stopped`, a primary length of a few µm (CSDA ≈ 2.5 µm), `primaryEkin0_keV = 10`, and `nSecondaryAll >= nSecondaryFirstGen`, with the same holding for the lengths. Serial and `--threads 2` give consistent tables.
- The new accumulator has a unit test (add/merge/clear/write/summary), passing under ctest in `build-ninja/`.

## Architecture & Approach

- **Hook:** `TrackingAction::PostUserTrackingAction` reads `G4Track::GetTrackLength()`, so no per-step code is needed. Track 1 gives its length, `GetVertexKineticEnergy()`, final KE and end reason. Every other e- adds its length and +1 to the all-generations totals, and also to the first-generation totals when its ParentID is 1.
- **End reason:** `killed` if `PrimaryKiller` set a per-thread "primary killed" flag when it killed track 1 (reset per event); `escaped` if `track->GetNextVolume() == nullptr`; `stopped` otherwise (solvation, or KE fell to the model limit).
- **Accumulator:** a new portable `scoring/` class with no Geant4 kernel dependency, in the style of `PhysicsInteractionCounter` (stream-only I/O, `Merge`/`Clear`/`WriteAscii`/`WriteCsv`, plus summary statistics). It holds per-event rows; the per-event values are gathered by TrackingAction and committed as one row in `EndOfEventAction` (aborted events dropped). Live per-thread counter, master-owned fallback, `Run::Merge`, then `RunAccumulator`, then written by `RunAccumulatorMessenger` at the Dump and reset.
- **Manifest:** `RunManifest::Write` adds `trackLengths` (one `Add` line plus a summary node built from the accumulator).
- **Units:** lengths in nm, energies in keV; column names carry the unit.
- **Docs:** CLAUDE.md key files, the `sim-output` skill, `docs/output/README.md`. Glossary terms **Primary track** and **Secondary electron** / **First-generation secondary electron** are already added to `CONTEXT.md`.

## Assumptions & Trade-offs

- One gun primary per event (track 1). Every other physical-stage track descends from it and is an e- under G4EmDNAPhysics. Non-electron secondaries, if any, are not counted.
- Track length is the full geometric path, including elastic and vibrational steps, so it isn't comparable one-to-one with the old proxy.
- Not doing: a per-step energy-deposit split between primary and secondaries (restricted LET), per-generation histograms beyond first/all, or chemistry-stage tracks.
- The `PrimaryKiller` box kill (`setSize`, 1 km by default) would also set the flag; that is accepted as `killed`.

## Open Questions

None.

## Notes

- Verified in source: `G4DNAOneStepThermalizationModel` kills a solvated electron while leaving its KE at 25 meV, so "KE == 0" can't mark "stopped". That is why PrimaryKiller flags its own kill.
- These scorers do the "true instrumented LET" that `track_length_let.py` lists as future work.
