# Add a per-run counter

## What this is

How to add a new plain accumulator — a portable, stream-only-I/O counter
tallying something across an event or a run (reaction firings, physics
interactions, per-event rows, …) that is *not* itself a `G4VPrimitiveScorer`
— and wire it into the `RunAccumulator` / `Run::Merge` / dump pipeline so it
persists across `/run/beamOn` calls and survives MT.

## Generic pattern

This project's counters are plain C++ classes with no Geant4-kernel
dependency (most are standard-library-only and explicitly
copy-paste-portable to another project — see the "Portable class
convention" note in each file's header comment), sharing one convention:

- **Stream-only I/O.** `WriteAscii(std::ostream&) const` and
  `WriteCsv(std::ostream&) const` (or, where the project's CSV output goes
  through `G4AnalysisManager` instead of a raw stream, `WriteCsv(G4VAnalysisManager*) const` —
  `ReactionCounter` does this because its CSV is an ntuple). No file paths,
  no `OutputDir` lookups, no `G4cout`, inside the counter itself; the caller
  (`RunAccumulatorMessenger`) owns the `std::ofstream`/`OutputDir::Resolve`
  calls and passes the stream in.
- **Mutating API.** `Add`/`Record` (names vary by what's natural for the
  tally — `TrackLengthTable::Add(const Row&)` appends one row,
  `ReactionCounter::Record(const G4String& reactionLabel, G4double time)`
  increments one bin/label pair) to feed one event's worth of data in,
  `Merge(const T& other)` to fold another instance's state into this one,
  `Clear()` to empty it.
- **Value semantics for `Merge`.** `Merge` always combines `other` *into*
  `this` and leaves `other` unchanged — never the reverse, never mutating
  the argument. `TrackLengthTable::Merge` (`src/scoring/TrackLengthTable.cc:29-32`)
  appends `other.fRows` onto `fRows`; `ReactionCounter::Merge`
  (`src/scoring/ReactionCounter.cc:131-138`) adds `other.fCounts[bin][label]`
  into `fCounts[bin][label]` per bin/label pair, which is also why `Merge`
  is safe to call repeatedly: it is pure addition, not a replace.

## dnachem-min reference implementation

Two actual examples, read from source rather than assumed from the class
names:

- **`src/scoring/ReactionCounter.cc`** (+ `header/scoring/ReactionCounter.hh`) —
  a per-time-bin, per-reaction-label firing count. `Record(label, time)`
  (`ReactionCounter.cc:126-129`) buckets `time` into a fixed or
  macro-configured set of bin edges (`BinFor`, `ReactionCounter.cc:115-124`)
  and increments `fCounts[bin][label]`. `Merge` (`:131-138`) adds bin/label
  counts from `other`. `Clear` (`:140-143`) empties `fCounts`. `WriteAscii`
  (`:145-153`) writes a plain `bin\nlabel    count = N\n` stream; `WriteCsv`
  (`:170-193`) takes a `G4VAnalysisManager*` and writes an ntuple (one row
  per bin/label pair) because this project's CSV convention goes through
  that manager rather than a raw `std::ostream` — the one place this
  counter's I/O isn't a plain stream, and the reason its signature differs
  from `TrackLengthTable::WriteCsv`.
- **`header/scoring/TrackLengthTable.hh` / `src/scoring/TrackLengthTable.cc`** —
  one `Row` per event (primary track length/energy/end-reason, secondary
  lengths and counts). `Add(const Row&)` (`TrackLengthTable.cc:24-27`)
  appends a row; `Merge(const TrackLengthTable& other)` (`:29-32`) appends
  `other.fRows`; `Clear` (`:34-37`) empties `fRows`; `WriteCsv`/`WriteAscii`
  (`:116-128`) write a header line plus one row per event, sorted by
  `(run, event)` via the private `Sorted()` helper — both take a plain
  `std::ostream&`, the more common case for this project's counters.
  `Summarize()` (`:71-96`) additionally folds the rows into a `Summary`
  (means and SEMs per column, counts per end-reason) for the manifest's
  `trackLengths` entry — a feature specific to this counter, not part of
  the shared convention.

Both are stream-only, dependency-free, and documented in their own file
header as copy-paste portable to another project (adjust the include path
prefix on copy) — see the "Portable class convention" memory note this
project follows.

## How a new counter gets wired in

Follow the existing counters' wiring exactly (`ReactionCounter` and
`TrackLengthTable` both go through every step below):

1. **Own the live instance on the per-thread action that feeds it.**
   `ReactionCounter` lives on `TimeStepAction` (`TimeStepAction::GetReactionCounter()`);
   `TrackLengthTable` lives on `TrackingAction`
   (`TrackingAction::GetTrackLengthTable()`), filled in
   `PostUserTrackingAction`. The action that naturally observes the data
   (a step, a track ending, a reaction firing) is where the live, persistent
   instance belongs — not `Run`, which is recreated every `/run/beamOn`.
2. **`Run` points at it, it does not own it.** `Run::Run()`
   (`src/actions/Run.cc:22-48`) looks up the owning action
   (`dynamic_cast<TimeStepAction*>(G4Scheduler::Instance()->GetUserTimeStepAction())`
   for `fReactionCounter`, `dynamic_cast<TrackingAction*>(...GetUserTrackingAction())`
   for `fTrackLengthTable`) and stores a pointer to its counter — falling
   back to a `Run`-owned instance (`fOwnedReactionCounter`, etc.) only if
   the action isn't found, which in practice only happens on the MT master
   thread, which has no `TimeStepAction`/`TrackingAction` of its own. A new
   counter's `Run` member follows the same pointer-not-owner pattern.
3. **`RunAction::BeginOfRunAction` clears it.** `RunAction::BeginOfRunAction`
   (`src/actions/RunAction.cc:50-59`) calls `Clear()` on every counter
   reached through the new `Run`, giving each counter a per-run lifetime —
   the comment there explains why this is the *only* reset: on the thread
   that owns the live action instance (Serial master, or an MT worker), the
   new `Run` points at that same live counter, so clearing it here resets
   what the action will go on to fill during this run; on the MT master
   (no owning action), it points at the `Run`-owned fallback instead, which
   starts empty anyway.
4. **`Run::Merge` combines worker into master under MT.** `Run::Merge`
   (`src/actions/Run.cc:84-112`) calls `fReactionCounter->Merge(*localRun->fReactionCounter)`
   and `fTrackLengthTable->Merge(*localRun->fTrackLengthTable)` (alongside
   `fInteractionCounter` and `fMesoSpeciesCounter`) — see
   [Serial vs MT](#serial-vs-mt) below for exactly when and how this runs.
   A new counter needs its own `Merge` call added here, or its counts will
   only ever reflect one worker's share under MT.
5. **`RunAction::EndOfRunAction` folds the run into `RunAccumulator`.**
   On the master thread (`IsMaster()`), `RunAction::EndOfRunAction`
   (`src/actions/RunAction.cc:92-119`) calls `RunAccumulator::Accumulate(...)`
   (`:104-108`), passing the master `Run`'s counters by const reference.
   `RunAccumulator::Accumulate` (`src/scoring/RunAccumulator.cc:22-34`)
   merges each into a process-wide, file-scope static instance
   (`gAccumulatedReactionCounter.Merge(reactions)`, etc.) that survives
   across `/run/beamOn` calls — this is what makes the counter's data
   persist across repeated `beamOn`s within one dump cycle, since `Run`
   itself is destroyed and recreated every `beamOn`. A new counter needs a
   new static instance, a new `Accumulate(...)` parameter, and a new
   `GetAccumulated<Counter>()` accessor here.
6. **`RunAccumulatorMessenger` writes it out and resets it.**
   `RunAccumulatorMessenger::WriteAllAndReset`
   (`src/scoring/RunAccumulatorMessenger.cc:123-275`) reads
   `RunAccumulator::GetAccumulated<Counter>()`, opens an
   `OutputDir::Resolve(...)`-ed `std::ofstream`, calls the counter's
   `WriteAscii`/`WriteCsv`, pushes the written filename(s) onto the dump's
   `files` list (for `Manifest.json`), then — at the end of the function —
   `RunAccumulator::ClearAccumulated()` (`:265`) clears every accumulated
   counter, including the new one, so the next dump cycle starts empty.
   `RunManifest::Write(prefix, subdir, files)` (`:263`) is what actually
   writes `Manifest.json` listing every file this dump produced; a
   counter's summary numbers (if any, like `TrackLengthTable::Summarize()`)
   go into the manifest through `RunManifest.cc`, not through this
   messenger directly — see that file if the new counter needs a manifest
   summary entry, not a `Txt`/`csv` file alone.

## The `G4UnitDefinition::GetValueOf()` pitfall

`ReactionCounter.cc`'s own comment (`ReactionCounter.cc:30-35`) documents a
real memory-corruption pitfall hit while building this class's time-unit
parsing (`ParseBinEdgesList`, used by `/chem/reaction/timeBinsList`):
`G4UnitDefinition::GetValueOf()` reaches into the precompiled Geant4 DLL and
is **unsafe to call from a Debug test binary linked against a differently-built
Geant4 install** (the project's test binaries build Debug in `build-ninja/`;
the installed Geant4 libraries used there are typically Release or
RelWithDebInfo) — observed to corrupt memory in that configuration. Because
`ReactionCounter` is deliberately kernel/DLL-free (no Geant4 kernel
dependency at all, by design — see the file's own header comment), it
avoids the whole class of bug by not calling `G4UnitDefinition::GetValueOf()`
in the first place: `TimeUnitValue` (`ReactionCounter.cc:36-51`) is a small,
local, dependency-free `std::map<G4String, G4double>` lookup instead,
mirroring Geant4's own time-unit names/symbols without touching the
Geant4 DLL. A new portable counter that needs to parse a unit string from a
macro argument should do the same — a local lookup table, not
`G4UnitDefinition::GetValueOf()` — to stay safe when its own unit test runs
from the Debug `build-ninja/` binary.

## Serial vs MT

This is the clearest, most-verified instance of the per-thread-accumulate /
master-merge pattern in this codebase; `create-a-scorer.md` and
`create-a-species-counter.md` link here rather than repeating it.

- **Serial** (`sim.cc` default, no `--threads`): there is exactly one
  thread, one `Run` instance per `/run/beamOn`, and the action classes
  (`TimeStepAction`, `SteppingAction`, `TrackingAction`) that own the live
  counters exist once for the whole job. `Run::Merge` is never called —
  `G4RunManager::RunInitialization`/`RunTermination` never invoke it in
  Serial mode, because there is nothing to merge. `RunAction::BeginOfRunAction`'s
  `IsMaster()` is always true (the one and only thread is the master), and
  `EndOfRunAction`'s `RunAccumulator::Accumulate(...)` call reads directly
  from the single `Run`'s live counters.
- **MT** (`--threads N`, N > 0, or `/run/numberOfThreads N`): each worker
  thread gets its own `Run` instance, constructed via `RunAction::GenerateRun()`
  on that thread, and its own `TimeStepAction`/`SteppingAction`/`TrackingAction`
  instances with their own live counters — `Run::Run()`'s lookups
  (`src/actions/Run.cc:22-48`) resolve to *that thread's* action objects via
  `G4Scheduler::Instance()`/`G4RunManager::GetRunManager()`, which are
  themselves per-thread (worker thread-local) singletons. Each worker
  accumulates independently through its event loop.

  The merge step is driven entirely by the Geant4 kernel, not by this
  project's code: **`G4MTRunManager::MergeRun(const G4Run* localRun)`**
  (`geant4SourcePath/source/run/src/G4MTRunManager.cc`, `MergeRun` method)
  calls `currentRun->Merge(localRun)` under a lock as each worker thread
  finishes its share of the run — `currentRun` is the master's `Run`
  instance, `localRun` is the worker's. This project's override,
  `Run::Merge(const G4Run* aRun)` (`src/actions/Run.cc:84-112`), is what
  that virtual call actually runs: it `dynamic_cast`s both sides'
  `fScorerRun` to `ScoreSpecies*` and calls
  `masterScorer->AbsorbResultsFromWorkerScorer(localScorer)` (`:98-102`),
  then calls `Merge` on each plain counter in turn —
  `fReactionCounter->Merge(*localRun->fReactionCounter)`,
  `fInteractionCounter->Merge(...)`, `fMesoSpeciesCounter->Merge(...)`,
  `fTrackLengthTable->Merge(...)` (`:106-109`) — and finally
  `G4Run::Merge(aRun)` (`:111`) to let the base class merge its own
  bookkeeping (event count, etc.).

  **Precise timing, the point this recipe is the canonical source for:**
  this kernel-driven merge happens **before** the master thread's
  `RunAction::EndOfRunAction(run)` runs with `IsMaster()` true
  (`src/actions/RunAction.cc:92`) — `G4MTRunManager::MergeRun` runs as part
  of run termination, as each worker reports back, which completes before
  `RunTermination()` calls the master's `EndOfRunAction`. `EndOfRunAction`
  does **not** itself trigger the merge; it only *observes* a `run` that
  the kernel has already merged by the time it runs. The ticket text that
  first motivated this recipe described the mechanism as "merge happens at
  `EndOfRunAction` when `IsMaster()`" — that phrasing conflates cause and
  effect: `IsMaster()` being true at `EndOfRunAction` is what lets this
  project safely read `masterRun->GetReactionCounter()` etc. and feed them
  into `RunAccumulator::Accumulate(...)` (`RunAction.cc:104-108`), but the
  merge itself already happened earlier, inside the kernel, driven by
  `G4MTRunManager::MergeRun`, not by anything in `RunAction`.

  A new counter added to `Run` needs its own `Merge(...)` call added inside
  `Run::Merge` the same way the four existing ones are — omitting it means
  the counter's cross-thread totals silently reflect only whichever
  worker's `Run` instance happened to become (or stay) the master's, with
  every other worker's contribution dropped, with no error or warning.

## See also

- [`create-a-scorer.md`](create-a-scorer.md) — the `G4VPrimitiveScorer`
  counterpart (`ScoreSpecies`/`AbsorbResultsFromWorkerScorer`); links here
  for the full `Run::Merge` timing rather than repeating it.
- [`create-a-species-counter.md`](create-a-species-counter.md) — likewise
  links here for the same mechanism applied to `ScoreSpecies`.
- `docs/output/README.md` and the `sim-output` skill — file formats,
  dump/prefix rules and the manifest, for the files a counter's
  `WriteAscii`/`WriteCsv` ultimately produce.
