# Implementation Plan

**Session:** physics-stage-output
**Date:** 2026-09-23T14:12:40.166Z
**Estimated effort:** 1 day

## Strategy

Two independent output additions, each wired through the existing
`ReactionCounter`/`TimeStepAction`/`Run`/`RunAction` pattern already used for
chemical reaction counts:

1. Total deposited energy — pure wiring, no new scoring (`Run::fSumEne` is
   already computed and merged correctly; it's just never written).
2. Physical interaction (process) firing counts — a new, fully portable
   `PhysicsInteractionCounter` fed by a new `SteppingAction` that filters
   G4DNA physics processes via a `"G4DNA"` substring match (verified against
   the Geant4 v11.4.1 source), wired through `Run`/`RunAction` exactly like
   `ReactionCounter`/`TimeStepAction`.

Ticket 1 builds the portable counter class in isolation (unit-testable on
its own, no Geant4 runtime). Ticket 2 (energy) has no dependency on Ticket 1
and can land independently. Ticket 3 depends on Ticket 1's counter class and
does all the Geant4-object wiring (`SteppingAction`, `Run`, `RunAction`),
verified end-to-end via a smoke run since none of its pieces are
independently observable before they're all connected. Ticket 4 documents
the finished behavior.

## Tickets Overview

- **01-physics-interaction-counter**: `PhysicsInteractionCounter` (portable
  string-frequency counter: `Record`/`Merge`/`Clear`/`WriteAscii`/`WriteCsv`,
  stream-only I/O, zero dnachem-min-specific includes) + CTest unit test +
  CMake wiring.
- **02-energy-deposit-output**: write the already-computed
  `Run::GetSumDose()` to `EnergyDeposit.Txt` + a `DnaLogger` Info line, in
  `RunAction::EndOfRunAction`.
- **03-physical-interaction-wiring**: `SteppingAction` (G4DNA process-name
  filter) + `Run`/`RunAction` wiring (counter pickup, merge/clear, output to
  `PhysicsInteractions.Txt`/`.csv`), registered in
  `ActionInitialization::Build()`.
- **04-docs**: update `CLAUDE.md`'s Key Files and output-files sections to
  document the three new/changed output files and the two new source files.

## Sequencing Rationale

01 first (no dependencies, unlocks 03). 02 can run any time after 01 (no
actual dependency on it, but keeping it second groups the "just wire an
existing value to a file" work before the larger new subsystem). 03 depends
on 01's `PhysicsInteractionCounter` class. 04 last, once the real output
filenames/behavior exist to document accurately.

## Risks & Mitigation

- **Process-name filter drift**: the `"G4DNA"` substring filter was verified
  against the Geant4 v11.4.1 source (`G4EmDNABuilder.cc`) for the processes
  this project's `G4EmDNAPhysics` actually registers. Mitigation: the smoke
  run in Ticket 3 explicitly checks for plausible per-process counts and the
  absence of a `Transportation` row, catching a filter regression
  immediately.
- **Double-counting risk if energy scoring were re-derived**: Ticket 2
  deliberately reuses `Run::fSumEne` instead of adding new accumulation in
  `SteppingAction`, avoiding drift from the already-verified
  `ScoreSpecies::ProcessHits` accumulation.
- **MT correctness**: both new/modified counters follow the exact
  `ReactionCounter`/`TimeStepAction` merge-then-clear pattern already
  proven correct for multi-threaded runs (worker counter merged into
  master, then cleared so it doesn't double-count across `/run/beamOn`
  calls).

## Assumptions

- No time binning for interaction counts (totals only, per explicit design
  decision) — no fixed/list bin-edge macro commands are added for this
  counter.
- Only discrete G4DNA physics interactions are counted; `Transportation` and
  other bookkeeping steps are excluded by construction.
- `PhysicsInteractionCounter`'s CSV header is `label,count` (not
  `process,count`) to keep the class free of domain-specific naming
  assumptions, consistent with its fully generic, portable design.

## Token Usage

- **Input:** 38
- **Output:** 44820
- **Cache read:** 2747341
- **Cache creation:** 65074
- **Total:** 2857273
