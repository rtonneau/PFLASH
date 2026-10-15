# Mesoscopic 3D species counting

## What this is

How species are counted after the particle-based chemistry stage hands over
to the mesoscopic stage: a Gillespie simulation (`G4DNAEventScheduler`) that
tracks molecule counts per cell on a 3D spatial mesh (`G4DNAMesh`), rather
than per diffusing track. "3D" here means that mesh — a regular grid of
cubic cells covering the chemistry boundary — not a visualization. This
recipe covers counting **aggregate per-cell species counts over time**
(`SpeciesMeso.*`); the separate per-event spatial snapshot of the mesh
itself (`SpeciesMesoSpatial.h5`, one 3D picture per record time per event)
is a different recipe — see
[`export-data-as-hdf5.md`](export-data-as-hdf5.md) rather than this page.

Why there is a mesoscopic stage at all, and the model it replaced (SBS /
classic IRT), is design rationale covered by
[`docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md`](../adr/0006-irt-syn-and-mesoscopic-chemistry.md)
and
[`docs/adr/0008-selectable-chemistry-modes.md`](../adr/0008-selectable-chemistry-modes.md)
— see those rather than this page for why, not how.

## Generic pattern

Geant4-DNA's `G4DNAEventScheduler` runs a Gillespie (stochastic) simulation
on a `G4DNAMesh`: molecules occupy cells, and the scheduler fires diffusive
jumps between neighbouring cells and bimolecular reactions within a cell,
advancing a global time. A `G4UserMeshAction` (`G4UserMeshAction.hh`) is the
scheduler's hook into this: `BeginOfMesh`/`InMesh`/`EndOfMesh` run around
and during every Gillespie step, each receiving the current `G4VDNAMesh*`
and the current time, so user code can read the mesh's per-cell molecule
counts at that instant. Reading the mesh and folding species counts into an
external counter — rather than relying on the scheduler's own counter map —
is what this recipe's `G4UserMeshAction` subclass does; the reasons to
bypass the scheduler's own map are in the dnachem-min section below.

## dnachem-min reference implementation

- `src/chemistry/TimeStepAction.cc` drives both chemistry stages per event.
  `StartProcessing` (`TimeStepAction.cc:388-458`) decides whether the
  mesoscopic stage is active this event (`fMesoOn`,
  `MesoSettings::StageEnabled`) and, if so, builds the log-spaced record
  times from the hand-over time to the end time
  (`MesoSettings::LogTimeGrid`, `/chem/meso/timesPerDecade`) and hands them
  to the mesh action via `StartRecording`. `UserPostTimeStepAction`
  (`TimeStepAction.cc:470-479`) watches the particle-based stage's global
  time and, the first time it reaches the hand-over time
  (`/chem/meso/handOverTime`, default 5 ns), calls `CompartmentBased()`
  (`TimeStepAction.cc:527-628`) exactly once per event (`fHandedOver`
  latches it).
- `CompartmentBased()` is the actual hand-over: it merges any live molecule
  whose species is also held by the bulk scavenger material into the bulk
  pool and kills the track (`MergeScavengerSpeciesIntoBulk`, a fix for a
  specific `G4DNAIndependentReactionTimeStepper` infinite loop — see the
  comment at `TimeStepAction.cc:491-503`), mutes the `G4MoleculeCounter`
  (the particle-stage counter behind `Species.*`; see
  [`create-a-species-counter.md`](create-a-species-counter.md)) for the
  rest of the event, then calls `fpEventScheduler->Initialize(...)` with a
  pixel count from `InitialPixel()` (`TimeStepAction.cc:483-487`, in turn
  from `MesoSettings::PixelCount`) and `fpEventScheduler->Run()`, which runs
  the Gillespie simulation to the end time.
- Cell mesh sizing is `MesoSettings`/`MesoMessenger`
  (`src/chemistry/MesoSettings.cc` + `header/chemistry/MesoSettings.hh`,
  `src/chemistry/MesoMessenger.cc`), pure/portable logic independent of the
  Geant4 kernel: `/chem/meso/voxelSize` (default 6.25 nm) is a *requested*
  cell size; `MesoSettings::PixelCount` picks the power-of-2 pixel count per
  side whose resulting cell size is closest to it, capped at 65536 because
  `G4DNAMesh::ConvertIndex` overflows a 32-bit index above that (ADR 0006) —
  on the default 1 mm box this gives 15.26 nm cells, not the requested
  6.25 nm, and `MesoMessenger`/`DnaChemistryList` warn via `DnaLogger` when
  the cap bites. `/chem/meso/handOverTime` and `/chem/meso/timesPerDecade`
  are the other two `MesoSettings::Values` fields read above.
- The actual counting happens in an anonymous-namespace
  `G4UserMeshAction` subclass, `MeshTotalsAction`
  (`TimeStepAction.cc:119-354`). Its `BeginOfMesh`/`InMesh` both call the
  private `Record(mesh, time)` (`TimeStepAction.cc:237-247`), which — once
  the mesh's current time has reached the next pending record time —
  snapshots every occupied cell's per-species counts
  (`MeshSpeciesCounts`, `TimeStepAction.cc:91-100`, summed over all cells)
  and stores that snapshot against every record time it has now reached (it
  may be several, if the simulation jumped past more than one record point
  in one step); `FinishRecording` (`TimeStepAction.cc:143-152`) fills any
  record time never reached with the mesh's final state once the scheduler
  stops at the end time. The class-level comment at
  `TimeStepAction.cc:102-118` explains **why** this snapshot-on-every-step
  approach replaces the scheduler's own built-in counter map: in Geant4
  11.4.1, `G4DNAEventScheduler::RecordTime()` only fires after a *reaction*
  step and records at most one record time per call, so late record times
  with few reactions read stale, frozen counts (the bug this fixed,
  `baf4b29`, mentioned in ADR 0006's validation section: O2 `e_aq` vs SBS
  went from +159% to +9% after the fix).
- `TimeStepAction::CollectMesoSpecies()` (`TimeStepAction.cc:659-673`), called
  from `EndProcessing()` at the end of every event, reads the mesh action's
  `Records()` map (`time -> species -> count`, not the scheduler's own
  counter) and folds each `(time, species)` pair into
  `fMesoSpeciesCounter` (a `MesoSpeciesCounter`,
  `header/scoring/MesoSpeciesCounter.hh`) via `Add(time/ns, name, n)`,
  skipping water and any bulk pseudo-species (`userID == "H2O"` or ending in
  `"(B)"`) — the same exclusion `Species.*` applies, so a species column
  means the same thing in both outputs. It then calls
  `fpEventScheduler->ResetCounter()` to rewind the scheduler for the next
  event.
- The `G4ThreadLocal` at `TimeStepAction.cc:358`,
  `G4ThreadLocal MeshTotalsAction *tMeshAction = nullptr;`, is a
  non-owning raw pointer to the current thread's mesh action (the owning
  `std::unique_ptr` is handed to `fpEventScheduler->SetUserMeshAction(...)`
  in the constructor, `TimeStepAction.cc:363-377`); it exists only so the
  free functions in the same anonymous namespace (`MeshTotal`, `MeshPixels`)
  and the class's own methods can reach "the mesh action for this thread"
  without threading a pointer through every call. See Serial vs MT below
  for why this matters.

## Output

`SpeciesMeso.Txt` and `SpeciesMeso.csv`
(`MesoSpeciesCounter::WriteAscii`/`WriteCsv`, `src/scoring/MesoSpeciesCounter.cc`)
are written by the same `/run/dumpDataAndReset` handler that writes
`Species.Txt` (`src/scoring/RunAccumulatorMessenger.cc`); they cover the
mesoscopic stage's record-time grid (hand-over time to end time) and are
not written at all when the mesoscopic stage is off
(`/chem/meso/enable false`, or the SBS time-step model). File formats,
column semantics, dump/prefix rules and the per-dump `Manifest.json` entry
are covered by [`docs/output/README.md`](../output/README.md) and the
`sim-output` skill — see those rather than this recipe for the output-file
conventions.

## Serial vs MT

Same `Run::Merge` mechanism as
[`create-a-scorer.md`](create-a-scorer.md#serial-vs-mt) and
[`create-a-species-counter.md`](create-a-species-counter.md#serial-vs-mt):
the merge is driven by the Geant4 kernel's `G4MTRunManager::MergeRun`,
*before* the master's `RunAction::EndOfRunAction` runs — see
`create-a-scorer.md`'s fuller explanation rather than re-deriving it here.

What is specific to this recipe is the `G4ThreadLocal` at
`TimeStepAction.cc:358`: under MT (`--threads N`), each worker thread gets
its own `TimeStepAction`, its own `G4DNAEventScheduler`, and — via that
thread-local pointer — its own `MeshTotalsAction`, so each worker's mesh
and record-time snapshots are entirely private to that thread; there is no
shared mesh state to protect. `Run::Run()`
(`src/actions/Run.cc:22-38`) looks up the current thread's `TimeStepAction`
off `G4Scheduler::Instance()->GetUserTimeStepAction()` and binds
`fMesoSpeciesCounter` to that instance's `GetMesoSpeciesCounter()`
(`Run.cc:34-35`); `Run::Merge` (`Run.cc:84-111`) then calls
`fMesoSpeciesCounter->Merge(*localRun->fMesoSpeciesCounter)`
(`Run.cc:108`) to fold one worker's per-thread counter into the master's,
the same per-run-counter pattern as `fReactionCounter`/`fInteractionCounter`/
`fTrackLengthTable` on the same lines. In Serial mode there is one thread,
one `TimeStepAction`, one `MeshTotalsAction` and no merge step.

## See also

- [`create-a-species-counter.md`](create-a-species-counter.md) — the
  particle-based stage's species counter (`Species.*`), which this stage's
  output continues in time.
- [`export-data-as-hdf5.md`](export-data-as-hdf5.md) — the separate
  per-event spatial snapshot of the mesh (`SpeciesMesoSpatial.h5`),
  `/chem/meso/spatialOutput`.
- [`docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md`](../adr/0006-irt-syn-and-mesoscopic-chemistry.md)
  — why the mesoscopic stage exists, the mesh-index overflow and its 65536
  cap, and the IRT_syn/SBS validation.
- [`docs/adr/0008-selectable-chemistry-modes.md`](../adr/0008-selectable-chemistry-modes.md)
  — how SBS, IRT_syn alone and IRT_syn + mesoscopic became selectable modes.
- [`docs/output/README.md`](../output/README.md) — `SpeciesMeso.Txt`/`.csv`
  file formats and dump conventions.
