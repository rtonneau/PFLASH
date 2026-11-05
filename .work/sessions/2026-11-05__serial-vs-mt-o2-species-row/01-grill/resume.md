# Session: serial-vs-mt-o2-species-row

**Date:** 2026-11-05T15:42:47.987Z
**Status:** Grill phase complete

## Problem Statement

With `/chem/env/scavenger O2` set and IRT_syn, Serial and MT give different chemistry for one species. Serial's `Species.*` has no `O_2^0` row at all (12 species). MT, even with `--threads 1`, has it (13 species, G about 0.014 at 1 ns). In pure water both modes have the row, and the other species agree.

The cause is not in the molecule counter or the table. On MT workers, `G4DNAMakeReaction` is built with a null scavenger material, so an `O2` product of two tracked molecules stays tracked there, while in Serial it joins the bulk pool.

## Context & Constraints

- **Current behavior:** `DnaChemistryList::ConstructProcess` installs the `G4DNAScavengerMaterial` after `G4DNAChemistryManager::Initialize()`. A worker builds the time-step model inside `Initialize()`; Serial builds it later, at the first event. `G4DNAMakeReaction` caches the material in its constructor (`G4DNAMakeReaction.cc:50`), so workers cache null and Serial caches the material. A runtime diagnostic confirmed this: Serial printed `ConstructTimeStepModel scavengerMaterial=set`, MT workers printed `scavengerMaterial=NULL`.

  Measured rows in `Species.csv` (10 keV e-, 5 events to 10 ns, except SBS with 2 events to 1 ns):

  | Configuration | Species | `O_2^0` rows |
  |---|---|---|
  | Serial, O2 21 % | 12 | 0 |
  | MT, 1 or 2 threads, O2 21 % | 13 | 37 |
  | Serial or MT, pure water | 13 | 37 |
  | Serial, SBS, O2 21 % | 13 | 61 |

- **Pain point:** Results depend on the run mode, in a configuration the project uses for its dissolved-O2 studies. Serial and MT do not run the same chemistry, however small the `O_2^0` yield is.
- **Dependencies:** The Geant4-DNA UHDR example installs the material in `ActionInitialization::Build()`, before the worker's `Initialize()`, so its model sees the material. On the master and in Serial the bulk composition is only populated inside `Initialize()`, so the install there must stay after that call. `TimeStepAction::MergeScavengerSpeciesIntoBulk` stays as a guard.
- **Tech stack:** C++20, Geant4 11.4.3 (DNA chemistry, `G4DNAMakeReaction`, `G4DNAChemistryManager`, `G4DNAScavengerMaterial`).

## Success Metrics

- With IRT_syn and O2 21 %, Serial and MT (1 and 2 threads) both give 12 species and 0 `O_2^0` rows in `Species.csv`.
- Pure water keeps its `O_2^0` row in Serial and MT, and SBS with O2 keeps its row.
- An MT + O2 run through the mesoscopic stage (about 20 events, 1 ms end time) completes without a fatal exception.
- `ctest` passes from `build-ninja`.

## Architecture & Approach

The user chose "Bulk pool in both": align MT on Serial and on the UHDR example, rather than keep radiolytic O2 tracked in both.

On a worker, install the material before `G4DNAChemistryManager::Initialize()`, because the master has already populated the shared composition by then. On the master and in Serial, keep installing it after `Initialize()`. The existing null check makes the second call a no-op on workers. Record the behavior in an addendum to ADR 0004.

## Assumptions & Trade-offs

- MT results with O2 set change, statistically: radiolytic O2 now feeds the bulk pool instead of being tracked and reacting as a track. Earlier MT + O2 runs are not comparable event by event.
- Tracked radiolytic O2 is given up under IRT_syn when bulk O2 is set, so `Species.*` does not report it.
- SBS is not changed: `G4DNAMolecularReaction` does not consult the material, so SBS + O2 still keeps a tracked `O2` and its row. This difference between SBS and IRT_syn stays, and ADR 0004 records it.
- With a non-null material, workers now also run the `SetEquilibrium` path of `G4DNAMakeReaction` (`:91`), as Serial already does. A code search found no Chemistry using reaction types 6-8, so no effect is expected.

## Open Questions

N/A

## Notes

A statistical comparison of the other species before and after the fix is not part of the success metrics. It would need at least 60 events per mode, as the earlier cross-mode check showed.
