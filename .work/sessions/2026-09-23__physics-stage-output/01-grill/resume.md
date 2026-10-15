# Session: physics-stage-output

**Date:** 2026-09-23T14:02:10.057Z
**Status:** Grill phase complete

## Problem Statement

The project currently outputs chemical-stage data only (species yields via
`Species.Txt`/CSVs, reaction firing counts via `Reactions.Txt`/CSVs). There is
no output for the physical stage: how many times each physics interaction
process fired during particle tracking, and the total energy deposited in the
simulation volume.

## Context & Constraints

- "Physical stage" = the pre-chemistry physics/tracking stage (electron
  transport, ionisation, excitation, etc., driven by `G4EmDNAPhysics` in
  `src/PhysicsList.cc`), as distinct from the chemical (diffusion-reaction)
  stage that `ReactionCounter`/`Reactions.Txt` already covers.
- `Run::fSumEne` (`Run::GetSumDose()`) already accumulates
  `G4Step::GetTotalEnergyDeposit()` for every step in the World volume via
  `ScoreSpecies::ProcessHits`, correctly merged across worker threads in
  `Run::Merge` — but it is never written to output today. No new scoring code
  is needed for total deposited energy, only wiring the existing value to a
  file + log line.
- No `G4UserSteppingAction` exists in the project yet (`SteppingAction` is a
  new file). No counter for physical interaction processes exists yet either.
- Verified in the Geant4 v11.4.1 source
  (`source/physics_lists/constructors/electromagnetic/src/G4EmDNABuilder.cc`)
  that every discrete DNA physics process `G4EmDNAPhysics` registers is named
  `<particle>_G4DNA<Type>` (e.g. `e-_G4DNAIonisation`, `e-_G4DNAExcitation`,
  `e-_G4DNAElastic`, `e-_G4DNAVibExcitation`, `e-_G4DNAAttachment`). Filtering
  on the `"G4DNA"` substring reliably selects these and excludes
  `Transportation`/other bookkeeping steps without hardcoding an exhaustive
  process list.
- User wants the new counter class to be as portable/reusable as possible,
  matching this codebase's existing convention for
  `src/PureWaterReactions.cc` ("portable, project-agnostic ... copy-paste
  portable to another project").
- Per user's earlier answers: interaction counts are **totals only** (no time
  binning, unlike `Reactions.Txt`); only **discrete physical interactions**
  count (i.e. the `"G4DNA"` substring filter), not every step's process
  (excludes `Transportation`); total deposited energy is reported **both** as
  a file and as a `DnaLogger` confirmation line.

## Success Metrics

- Running `sim beam.in` (25 keV e-, `/run/beamOn 2`, per project testing
  convention) produces, alongside the existing Species/Reactions outputs:
  - `EnergyDeposit.Txt`: one human-readable line with the total energy
    deposited in the simulation volume (`G4BestUnit`), plus a
    `[RunAction] ...` confirmation line at `DnaLogger` Info level.
  - `PhysicsInteractions.Txt`: one `"<processName>    count = N"` line per
    distinct G4DNA physics process that fired, mirroring `Reactions.Txt`'s
    per-line style.
  - `PhysicsInteractions.csv`: `process,count` header + one row per process,
    plain-stream CSV (no `G4AnalysisManager`/ntuple machinery — there is no
    time/event dimension to this table).
- `--dir`/`/run/outputDir` redirection still applies to both new outputs,
  matching all other output files (`OutputDir::Resolve`).
- `PhysicsInteractionCounter` has a unit test under `test/` (plain `assert` +
  CTest, built/run from `build-ninja/`), mirroring
  `test/ReactionCounterTest.cc`.
- Multiple `/run/beamOn` in one macro behaves the same as the existing
  Species/Reactions files (overwrite, `_bis` CSV naming) — no special-casing
  needed since `RunAction::EndOfRunAction` writes all output files the same
  way each time it runs.

## Architecture & Approach

Two independent pieces, following the existing `ReactionCounter`/
`TimeStepAction`/`Run` wiring pattern used for chemical reactions:

**1. Total deposited energy (wiring only, no new scoring):**
`RunAction::EndOfRunAction` gains a new block, alongside the existing
Species/Reactions blocks, that writes `masterRun->GetSumDose()` to
`OutputDir::Resolve("EnergyDeposit.Txt")` via `G4BestUnit(..., "Energy")`,
then logs a `[RunAction] energy deposit written (EnergyDeposit.Txt)`-style
line via `DnaLogger::Print(DnaLogger::Level::Info, ...)`.

**2. Physical interaction (process) firing counts (new subsystem):**

- `PhysicsInteractionCounter` (new `header/PhysicsInteractionCounter.hh` +
  `src/PhysicsInteractionCounter.cc`): a fully generic string-frequency
  counter — `map<G4String, G4long>` (or similar), with `Record(label)`,
  `Merge(other)`, `Clear()`, `WriteAscii(std::ostream&)`,
  `WriteCsv(std::ostream&)`. No `OutputDir`/`DnaLogger`/any dnachem-min
  project includes — stream-only I/O, so it is copy-paste portable to
  another project (stricter than `ReactionCounter`, which still includes
  `OutputDir.hh`). It has no notion of "process" or "G4DNA" at all; it just
  counts occurrences of whatever label callers pass to `Record()`. Doc
  comment at the top states this portability intent explicitly, matching
  `PureWaterReactions.cc`'s existing convention.
- `SteppingAction` (new `header/SteppingAction.hh` + `src/SteppingAction.cc`,
  a `G4UserSteppingAction`): owns a `PhysicsInteractionCounter`. In
  `UserSteppingAction(const G4Step*)`, reads
  `step->GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName()` and
  calls `Record()` only when the name contains `"G4DNA"` — this filter is a
  one-line private helper documented as holding for any
  `G4EmDNAPhysics`-based project (project-glue, not part of the portable
  counter class). Registered in `ActionInitialization::Build()` alongside
  `EventAction`/`StackingAction`/`TrackingAction` (worker/serial thread only,
  not `BuildForMaster()` — same as `TimeStepAction`/`ReactionCounter`).
- `Run` wiring mirrors the existing `ReactionCounter`/`TimeStepAction`
  pattern exactly: in `Run::Run()`, pick up the live counter via
  `const_cast<SteppingAction*>(dynamic_cast<const SteppingAction*>(
  G4RunManager::GetRunManager()->GetUserSteppingAction()))` (worker thread),
  falling back to an owned `PhysicsInteractionCounter` instance on the master
  thread (merge-accumulation target only, same role as
  `fOwnedReactionCounter`). `Run::Merge` merges the worker's counter into the
  master's and clears the worker's copy afterward (it persists across
  `/run/beamOn` calls otherwise, same reasoning as `ReactionCounter`).
- `RunAction::EndOfRunAction` opens `OutputDir::Resolve("PhysicsInteractions.Txt")`
  and `OutputDir::Resolve("PhysicsInteractions.csv")` as `std::ofstream`s and
  passes them to the counter's `WriteAscii`/`WriteCsv` — the counter itself
  never resolves paths. Logs a `[RunAction] physical interaction counts
  written (PhysicsInteractions.Txt / PhysicsInteractions.csv)` line via
  `DnaLogger` at Info level, matching the existing Reactions confirmation
  line's style.

## Assumptions & Trade-offs

- No time binning for interaction counts (per user's explicit choice) — if
  finer-grained (per-time-bin, like `ReactionCounter`) physical-interaction
  data is ever needed, that would need a follow-up design; this scope is
  totals-only.
- The `"G4DNA"` substring filter is a pragmatic, verified-against-source
  choice over enumerating exact process names, trading a small chance of
  over-inclusion (any future G4DNA-named process not previously considered
  gets counted automatically) for robustness against `G4EmDNAPhysics` option
  changes. This matches the project's existing "verify against source"
  ground rule rather than guessing process names.
- No id/metadata-mapping file (unlike `Reactions.Txt`/`ReactionsMetadata.csv`)
  since `PhysicsInteractions.csv` rows use the process name directly as the
  key — there is no per-time-bin repetition to compact away.
- Total deposited energy reuses the existing `Species`-scorer-derived
  `Run::fSumEne`; it is not re-derived from `SteppingAction`, since the
  scoring already exists, is already correctly merged, and duplicating it in
  `SteppingAction` would double-count or risk drifting from the
  already-verified `ScoreSpecies::ProcessHits` accumulation.

## Open Questions

None outstanding — all resolved during brainstorming (physical-process
counting vs. chemical reactions; totals-only vs. time-binned; discrete
physics interactions vs. every step process; energy output as file+log; the
portability split for `PhysicsInteractionCounter`).

## Notes

Relevant existing files to follow as patterns: `src/ReactionCounter.cc` /
`header/ReactionCounter.hh` (counter shape, though this new class drops its
`OutputDir` coupling), `src/PureWaterReactions.cc` (portability-doc-comment
convention), `src/Run.cc` / `header/Run.hh` (per-thread counter pickup +
merge/clear pattern), `src/RunAction.cc` (`EndOfRunAction` output-writing
block style, `DnaLogger` confirmation lines), `test/ReactionCounterTest.cc`
(unit test style/build for the new counter's test).

## Token Usage

- **Input:** 74
- **Output:** 31472
- **Cache read:** 3151258
- **Cache creation:** 66216
- **Total:** 3249020
