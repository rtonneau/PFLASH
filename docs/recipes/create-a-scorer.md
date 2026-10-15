# Create a scorer

## What this is

How to add a new `G4VPrimitiveScorer` (a hit-level tally driven by the
stepping/tracking loop through a sensitive detector) and register it so the
run manager drives it through its lifecycle and merges it correctly in MT
mode.

## Generic pattern

A scorer subclasses `G4VPrimitiveScorer`
(`geant4SourcePath/source/digits_hits/detector/include/G4VPrimitiveScorer.hh`)
and implements the one pure-virtual method:

- `virtual G4bool ProcessHits(G4Step*, G4TouchableHistory*) = 0;` (protected,
  `G4VPrimitiveScorer.hh:115`) — called once per step that reaches the
  sensitive detector. This is where a scorer reads the `G4Step` and
  accumulates whatever it tallies (energy, a species count, a track length,
  …).
- `virtual void Initialize(G4HCofThisEvent*)` and
  `virtual void EndOfEvent(G4HCofThisEvent*)` (public, `G4VPrimitiveScorer.hh:70-71`)
  — the two other lifecycle hooks a scorer can override: `Initialize` runs at
  the start of each event, `EndOfEvent` at its end (e.g. to fold per-event
  state into a persistent total). Both have no-op default bodies in the base
  class; overriding them is optional.

These three methods are "invoked by `G4SDManager` through
`G4MultiFunctionalDetector`", per the header's own comment
(`G4VPrimitiveScorer.hh:69`): a scorer never calls them itself. The actual
call site is `G4MultiFunctionalDetector::ProcessHits`
(`geant4SourcePath/source/digits_hits/detector/src/G4MultiFunctionalDetector.cc`,
the `ProcessHits` method), which — only when the step has nonzero length or
energy deposit — loops over every registered primitive and calls
`pr->HitPrimitive(aStep, aTH)`; `HitPrimitive` (private, `G4VPrimitiveScorer.hh:139-145`)
applies the scorer's optional `G4VSDFilter` and then calls `ProcessHits`.
`G4MultiFunctionalDetector::Initialize`/`EndOfEvent` likewise fan out to
every registered primitive's own `Initialize`/`EndOfEvent`.

A scorer is attached to the geometry by registering it on a
`G4MultiFunctionalDetector`, which is itself a `G4VSensitiveDetector` set on
a logical volume:

```cpp
G4bool G4MultiFunctionalDetector::RegisterPrimitive(G4VPrimitiveScorer* aPS);
```

(`geant4SourcePath/source/digits_hits/detector/include/G4MultiFunctionalDetector.hh:54`).
`RegisterPrimitive` appends the scorer to the detector's internal
`primitives` vector, calls `aPS->SetMultiFunctionalDetector(this)`, and (if
the detector is already registered with `G4SDManager`) registers the
scorer's hits-collection name with `G4SDManager::AddNewCollection` — multiple
primitives can share one `G4MultiFunctionalDetector`, each getting its own
named hits collection. One `G4MultiFunctionalDetector` is then handed to
`G4VUserDetectorConstruction::SetSensitiveDetector` for a named logical
volume, in `ConstructSDandField()`.

## dnachem-min reference implementation

`DetectorConstruction::ConstructSDandField()`
(`src/geometry/DetectorConstruction.cc:124-146`) is this project's
registration site: it creates one `G4MultiFunctionalDetector("mfDetector")`,
registers two primitives on it — `PrimaryKiller` and
`ScoreSpecies("Species")` — via `mfDetector->RegisterPrimitive(...)`, then
calls `G4SDManager::GetSDMpointer()->AddNewDetector(mfDetector)` and
`SetSensitiveDetector("World", mfDetector)`. Both primitives therefore run
off the same step stream through the `World` logical volume; `ScoreSpecies`
is the Geant4-DNA-specific species counter built on this mechanism — see
[`create-a-species-counter.md`](create-a-species-counter.md) for how it
actually counts, rather than repeating that here.

A new scorer or counter file belongs under `src/scoring/` (+
`header/scoring/`), per the clustered source layout — see
[`docs/adr/0003-clustered-source-layout.md`](../adr/0003-clustered-source-layout.md)
for why, rather than repeating it here.

## Serial vs MT

- **Serial** (`sim.cc` default, no `--threads`): one `Run` object and one
  scorer instance exist for the whole job. `ProcessHits`/`EndOfEvent` accumulate
  directly into that single instance; there is no merge step.
- **MT** (`--threads N` or `/run/numberOfThreads N`): each worker thread gets
  its own `Run` and its own scorer instance, accumulating independently.
  Merging onto the master happens through `G4Run::Merge`, which this
  project's `Run::Merge` override implements
  (`src/actions/Run.cc:84-111`): it `dynamic_cast`s both the master's and the
  worker's `fScorerRun` to `ScoreSpecies*` and calls
  `masterScorer->AbsorbResultsFromWorkerScorer(localScorer)`
  (`Run.cc:98-102`), alongside merging the other per-run counters
  (`fReactionCounter`, `fInteractionCounter`, `fMesoSpeciesCounter`,
  `fTrackLengthTable`) and finally `G4Run::Merge(aRun)`
  (`Run.cc:106-111`). In the Geant4 kernel this merge is driven by
  `G4MTRunManager::MergeRun(const G4Run* localRun)`, which calls
  `currentRun->Merge(localRun)` under a lock
  (`geant4SourcePath/source/run/src/G4MTRunManager.cc`, `MergeRun` method) as
  each worker finishes its run — so by the time the master thread's
  `RunAction::EndOfRunAction(run)` runs with `IsMaster()` true
  (`src/actions/RunAction.cc:92`), `run` already reflects every worker's
  contribution. A new scorer added to `ConstructSDandField()` needs its own
  merge step wired into `Run::Merge` the same way, or its counts will only
  ever reflect one worker's share under MT.

## See also

- [`create-a-species-counter.md`](create-a-species-counter.md) — the
  Geant4-DNA-specific layer: how `ScoreSpecies` actually counts species.
- [`docs/adr/0003-clustered-source-layout.md`](../adr/0003-clustered-source-layout.md)
  — where a new scorer file belongs.
- [`add-a-per-run-counter.md`](add-a-per-run-counter.md) — the related
  pattern for a plain accumulator (not a `G4VPrimitiveScorer`) fed from
  `RunAction::EndOfRunAction`.
