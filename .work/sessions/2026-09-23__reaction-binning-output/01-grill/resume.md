# Session: reaction-binning-output

**Date:** 2026-09-23T09:33:50.895Z
**Status:** Grill phase complete

## Problem Statement

The existing per-reaction bimolecular reaction counter (`ReactionCounter`, driven from
`TimeStepAction::UserReactionAction`, written out by `RunAction::EndOfRunAction`) has three
limitations:

1. Its time-bin edges are a hardcoded 7-entry constant (`kTimeBins` in `ReactionCounter.cc`) --
   there is no way to choose a fixed bin width or a custom set of bin edges from a macro.
2. `Reactions_nt_reactions.csv` repeats the full reaction label string on every row, and there
   is no compact way to correlate rows across a single reaction without re-parsing that string.
3. `RunAction::EndOfRunAction` prints a status line via a raw `G4cout`, so it always appears on
   stdout regardless of the project's `DnaLogger` verbosity setting.

## Context & Constraints

- Existing flow (not being replaced, only extended): `TimeStepAction::UserReactionAction` records
  every fired reaction into a `ReactionCounter` (owned per-thread by `TimeStepAction`, or by `Run`
  in Serial mode); `Run::Merge` merges worker counters into the master's; `RunAction::EndOfRunAction`
  (master only) writes `Reactions.Txt` (ASCII, via `ReactionCounter::WriteAscii`) and
  `Reactions_nt_reactions.csv` (via `ReactionCounter::WriteCsv` + `G4AnalysisManager`).
- `OutputDir::Resolve(filename)` already redirects every output file into the `--dir <path>` folder
  when passed; this feature does not add a new subfolder, it keeps using that same mechanism.
- `DnaLogger` (`src/DnaLogger.cc`) already provides leveled logging; default level is `Quiet`, so
  routing a print through `DnaLogger::Print(Level::Info, ...)` makes it silent by default without
  removing the diagnostic entirely.
- The project's established macro-config pattern is `G4GenericMessenger` under a `PreInit`-only UI
  directory owned by a single, MT-shared instance -- e.g. `DnaChemistryWorld`'s `/chem/env/pH` and
  `/chem/env/O2`, and `DnaChemistryList`'s existing `/chem/reaction/dump`. `DnaChemistryList` is
  constructed once by `PhysicsList` and that one instance is shared across the master and all
  worker threads (not duplicated per-thread the way `RunAction`/`TimeStepAction` are), so adding UI
  commands there does not risk duplicate-registration errors.
- The chemistry time limit (`G4Scheduler::Instance()->SetEndTime(...)`) defaults to 1 microsecond,
  set in `ActionInitialization::Build()` on `/run/initialize`, and can be overridden by
  `/scheduler/endTime <value> <unit>` issued after `/run/initialize` but before `/run/beamOn`. Any
  fixed-width bin generation that depends on the scheduler end time must therefore happen no earlier
  than `RunAction::BeginOfRunAction` (which fires on `/run/beamOn`, after any such override), not at
  macro-command time (`PreInit`).
- `RunAction::BeginOfRunAction` is called once per thread (guarded `IsMaster()` sections already
  exist in this file for output/console work), so any global mutable state written from multiple
  threads (the shared reaction-binning edge table) must be either master-only or synchronized.

## Success Metrics

- A macro can select either a fixed bin width (`/chem/reaction/timeBinsFixed <width> <unit>`) or an
  explicit list of bin edges (`/chem/reaction/timeBinsList <e1> <e2> ... <eN> <unit>`); if neither is
  issued, behavior is unchanged from today (the current hardcoded 7-edge table).
- Reaction-count output is two files: a single CSV with `(reactionId, time, count)` rows, and a
  metadata CSV mapping `reactionId -> reaction label`. Both still honor `--dir`/`OutputDir::Resolve`
  like every other output file in the project.
- No reaction-count-related text reaches stdout by default; the existing status line is only visible
  at `/dnaLogger/verbose Info` or above.
- Existing `ReactionCounterTest.cc` unit tests keep passing unmodified (default binning behavior is
  preserved), plus new unit tests cover the configurable-edges and reaction-id-mapping logic.

## Architecture & Approach

**1. Configurable binning**

- `ReactionCounter` (`header/ReactionCounter.hh` / `src/ReactionCounter.cc`): replace the anonymous-
  namespace `kTimeBins` constant with mutable module-level state (still defaulting to the current
  7 edges), guarded by a `std::mutex` so concurrent worker threads can safely read (`BinFor`) while
  another thread writes (`ConfigureBinEdges`). Add:
  - `static void ConfigureBinEdges(const std::vector<G4double>& edges);` -- replaces the active edge
    table (sorted ascending, de-duplicated); an empty vector is a no-op.
  `BinFor`'s signature and clamp-to-last-edge behavior are otherwise unchanged.

- `DnaChemistryList` (`header/DnaChemistryList.hh` / `src/DnaChemistryList.cc`): extend the existing
  `/chem/reaction/` `G4GenericMessenger` (already hosting `dump`) with two new `PreInit` commands:
  - `timeBinsFixed <width> <unit>` -- a value+unit command (`DeclareMethodWithUnit`), stores the
    width and marks the mode as "fixed".
  - `timeBinsList <e1> ... <eN> <unit>` -- a single string command (`DeclareMethod` on a `G4String`
    setter; G4UIcommand passes the whole remainder of the line as one token for a lone/last string
    parameter), parsed by splitting on whitespace: the last token is resolved via
    `G4UnitDefinition::GetValueOf`, the rest are doubles multiplied by that unit value, then sorted;
    marks the mode as "list".
  - New `void ApplyReactionTimeBinning() const;`: if mode is "fixed", expands the width into concrete
    edges up to `G4Scheduler::Instance()->GetEndTime()` and calls
    `ReactionCounter::ConfigureBinEdges`; if mode is "list", passes the already-parsed edges through
    directly; if neither command was ever issued, does nothing (default table stands).

- `PhysicsList` (`header/PhysicsList.hh`): add `const DnaChemistryList* GetChemistryList() const`
  (mirrors the existing `IsChemistryEnabled()` accessor) so `RunAction` can reach the single shared
  `DnaChemistryList` instance.

- `RunAction::BeginOfRunAction` (`src/RunAction.cc`): guarded by `IsMaster()` (matching the existing
  guarded sections in this file), look up the physics list via
  `G4RunManager::GetRunManager()->GetUserPhysicsList()`, cast to `PhysicsList`, and call
  `GetChemistryList()->ApplyReactionTimeBinning()` -- runs once per run, before any event/reaction
  processing starts, safely after any `/scheduler/endTime` override.

**2. Output layout**

- `ReactionCounter::WriteCsv` is rewritten to first build a `label -> reactionId` map by sorting all
  distinct reaction labels seen across every bin (`0, 1, 2, ...`), then write ntuple rows as
  `(reactionId [int], time [double], count [int])` instead of today's `(reaction [string], time,
  count)`.
- New `ReactionCounter::WriteMetadata(std::ostream&) const` writes a plain `reactionId,reaction` CSV
  (same plain-`ostream` style as `WriteAscii`, not routed through `G4AnalysisManager`, since it's a
  simple two-column dump built once at end-of-run on the master thread only).
- `RunAction::EndOfRunAction` calls `WriteMetadata` against a new file resolved via
  `OutputDir::Resolve("ReactionsMetadata.csv")`, alongside the existing `Reactions.Txt` /
  `Reactions_nt_reactions.csv` writes.
- `Reactions.Txt` (ASCII) is unaffected -- it keeps printing the full label directly.

**3. Stdout**

- The `G4cout << "[RunAction] reaction counts written..."` line in `RunAction::EndOfRunAction` is
  replaced with `DnaLogger::Print(DnaLogger::Level::Info, "...")`, updated to mention all three
  output files (`Reactions.Txt` / `Reactions_nt_reactions.csv` / `ReactionsMetadata.csv`).

**4. Docs**

- Touch up `CLAUDE.md`'s reaction-counting section (Key Files / Macro and Logging) to document the
  two new `/chem/reaction/timeBinsFixed` and `/chem/reaction/timeBinsList` commands and the new
  `ReactionsMetadata.csv` output file.

**5. Tests**

- `test/ReactionCounterTest.cc`: add tests for `ConfigureBinEdges` (custom edges override `BinFor`;
  empty vector is a no-op) and for the reaction-id-mapping logic used by `WriteCsv`/`WriteMetadata`
  (exposed as a small pure helper if needed to keep it testable without a `G4AnalysisManager`).

## Assumptions & Trade-offs

- `timeBinsFixed` and `timeBinsList` are mutually exclusive; issuing one after the other means "last
  one wins" (no explicit error for setting both), consistent with how simple `G4GenericMessenger`
  properties in this project already behave (e.g. re-issuing `/chem/env/pH`).
- The reaction-id assignment (alphabetical order of distinct labels, seen this run) is not a fixed
  cross-run registry -- if the set of reactions that fire differs between two runs (e.g. because of
  scavenger settings), the same reaction may get a different id in each run. This is fine because
  `ReactionsMetadata.csv` is written fresh every run and is the authoritative id->label mapping for
  that run's `Reactions_nt_reactions.csv`.
- No new output subfolder is introduced -- reaction files continue to sit at the top level of
  whatever `--dir` points to (or cwd), exactly like `Reactions.Txt` does today. This was an explicit
  user choice: the existing `--dir` mechanism already satisfies "dedicated folder".
- Only one CSV layout (shared file + metadata) is implemented, not the per-reaction-file alternative
  that was also floated -- this was an explicit user choice, not a limitation to revisit later.

## Open Questions

None outstanding -- all points above were resolved during brainstorming (binning UI shape, fixed-mode
range semantics, CSV layout choice, stdout handling, and dedicated-folder scope).

## Notes

Session: `2026-09-23__reaction-binning-output`. This is a bounded change (extends the existing,
already-shipped `ReactionCounter`/`RunAction`/`OutputDir`/`DnaLogger` flow) -- no `02-plan/plan.md`
or tickets; implementation proceeds directly from this resume via the normal dev workflow (TDD for
`ReactionCounter` changes), then `/gps finish`.

## Token Usage

- **Input:** 76
- **Output:** 33392
- **Cache read:** 3418794
- **Cache creation:** 77718
- **Total:** 3529980
