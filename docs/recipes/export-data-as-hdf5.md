# Export data as HDF5

## What this is

How to write per-event, structured, possibly large binary output (spatial
snapshots, histograms, anything a `.Txt`/`.csv` row doesn't fit well) from a
Geant4-DNA app to HDF5, following the same staging/move/manifest convention
the project's ASCII/CSV output uses, rather than inventing a new one. This is
not about *what* to put in the file — that is application logic — but about
*when* the file is written relative to the event loop and `/run/
dumpDataAndReset`, and how that differs from the counter pattern used
elsewhere in this project.

## Generic pattern

Geant4-DNA runs are driven event by event (and, under MT, worker thread by
worker thread), but output is meant to be dumped in batches, on demand, via
`/run/dumpDataAndReset` — not once per event and not only at end of run. For
ASCII/CSV counters (`ReactionCounter`, `MesoSpeciesCounter`, …) this project
solves that by keeping the data in memory (summed per worker thread) until a
dump writes and clears it; see
[`create-a-species-counter.md`](create-a-species-counter.md) and
[`mesoscopic-3d-species-counting.md`](mesoscopic-3d-species-counting.md). An
HDF5 file holding one event's worth of data per group cannot wait in memory
the same way without growing without bound for a long run, and an HDF5 file
cannot be "merged" from two independent in-memory halves the way two
`std::map` counters can — it is a single shared file format with internal
bookkeeping (its own b-tree/heap structures), not a flat buffer. The pattern
that works instead:

1. **Stage.** Each event that has data appends its own groups directly to one
   on-disk HDF5 file in a private, hidden staging directory, as soon as the
   event finishes — not buffered in memory, so a long run can't blow up
   process memory, and not written straight into the configured output
   directory, so a reader never sees a half-written file.
2. **Move at dump time.** When a macro issues `/run/dumpDataAndReset [prefix]`
   or `/run/dumpDataAndResetToDir <subdir>`, the staged file is renamed
   (`std::filesystem::rename`, not copied) into the actual dump target and
   listed in that dump's `Manifest.json`. A rename is atomic on the same
   filesystem and cheap regardless of file size — unlike the counters, there
   is no in-memory state to serialise at dump time, only a file move.
3. **Safety net.** The same end-of-process safety-net flush that catches
   un-dumped counters (prefix `EndOfRun_`, see `docs/output/README.md`) also
   moves the staged HDF5 file, so data staged since the last dump is never
   silently dropped if the macro forgets a final `/run/dumpDataAndReset`.

Appending groups to an existing HDF5 file, rather than writing a new file per
event, keeps the file count manageable for runs with many events and lets a
reader iterate `/run<R>/event<E>/...` groups in one open. HDF5's own
C++ API (`H5::H5File`, opened `H5F_ACC_RDWR` or `H5F_ACC_EXCL` depending on
whether the file exists yet) handles the actual group/dataset creation; this
recipe is about the surrounding staging/move/validate logic, not the HDF5 API
itself.

## dnachem-min reference implementation

`src/scoring/MesoSpatialFile.cc` (+ `header/scoring/MesoSpatialFile.hh`) is
this project's only HDF5 writer, serialising the mesoscopic stage's per-event
spatial snapshots (`/chem/meso/spatialOutput`) to `SpeciesMesoSpatial.h5`. It
is marked portable (standard library and HDF5 C++ only, no dependency on
other dnachem-min classes beyond the data it's handed — adjust the include
path prefix on copy), so it is the template to copy for a new kind of
per-event HDF5 data, not a one-off tied to the mesoscopic stage. Verified
against the actual source:

- **Staging path.** `MesoSpatialFile::StagingDir(outputDir)`
  (`MesoSpatialFile.cc:128-132`) returns
  `outputDir + "/.pending_meso_spatial"` (or `.pending_meso_spatial` with no
  `--dir`), mirroring the existing `.pending_prechem` convention
  (`PreChemicalFiles`) rather than introducing a different staging
  directory name pattern. `StagedPath` appends the fixed file name
  (`FileName()`, `"SpeciesMesoSpatial.h5"`).
- **Append per event.** `TimeStepAction::WriteSpatialSnapshots()`
  (`src/chemistry/TimeStepAction.cc:634-653`) runs once per event, right
  after the mesoscopic stage finishes recording that event's snapshots: it
  skips entirely when there are no records for this event
  (`data.records.empty()`, `TimeStepAction.cc:638`), fills `runId`/`eventId`
  from the current `G4Run`/`G4Event`, and calls
  `MesoSpatialFile::AppendEvent(MesoSpatialFile::StagedPath(OutputDir::GetDirectory()), species, data, err)`
  (`TimeStepAction.cc:644-645`). A write failure is a `JustWarning`
  `G4Exception`, not fatal — one event's spatial data is lost, but the run
  continues (`TimeStepAction.cc:646-650`).
- **What `AppendEvent` actually does**
  (`MesoSpatialFile.cc:139-224`, verified line by line): validates the event
  data shape first (`Validate`, `MesoSpatialFile.cc:97-120` — `position_nm`
  size a multiple of 3, `counts` sized cells × species, every record's
  snapshot index in range) before touching the file at all; creates the
  staging folder if missing; opens the file `H5F_ACC_RDWR` if it already
  exists or `H5F_ACC_EXCL` (fails if concurrently created) if not; on first
  creation writes the root attributes (`species`, `formatVersion`,
  `formatDoc`, `units`); on reopen, checks the existing `species` attribute
  matches exactly and that `/run<R>/event<E>` doesn't already exist, refusing
  to double-write or silently merge mismatched species lists; then creates
  `/run<R>/event<E>/snapshot<k>/` groups, writing `position_nm`/`counts`
  datasets once per distinct mesh state and hard-linking repeats
  (`MesoSpatialFile.cc:192-216`) rather than duplicating identical data —
  specific to this writer's own schema, not a generic requirement of the
  staging pattern.
- **Move at dump time.** `RunAccumulatorMessenger`'s `/run/dumpDataAndReset`
  handler (`src/scoring/RunAccumulatorMessenger.cc:243-259`) calls
  `MesoSpatialFile::MoveStaged(OutputDir::GetDirectory(), OutputDir::Resolve(MesoSpatialFile::FileName()), spatialMoved, spatialErr)`
  after moving the staged pre-chemical files and before writing
  `Manifest.json`; on success with `spatialMoved == true` it appends the
  (prefix-applied) file name to that dump's `files` list. `MoveStaged`
  itself (`MesoSpatialFile.cc:226-244`) is a no-op returning `true` with
  `moved = false` when nothing was staged — not every dump interval
  necessarily has spatial output — and otherwise creates the destination's
  parent directory, removes any pre-existing file at the destination, and
  `fs::rename`s the staged file onto it, so a dump always gets a clean move
  rather than an accumulating append across dumps.
- **Safety net.** The same `FlushIfPending()` call from `sim.cc` that catches
  un-dumped counters at process exit (prefix `EndOfRun_`) reaches this same
  `RunAccumulatorMessenger` dump path, so a staged `SpeciesMesoSpatial.h5`
  never manually dumped is still moved out of `.pending_meso_spatial/`
  before the process exits.
- **Layout and format.** The exact group/attribute/dataset layout
  (`/run<R>/event<E>/snapshot<k>/`, datasets `position_nm`/`counts`,
  attributes `time_ns`/`cellSize_nm`) is
  [`docs/output/SpeciesMesoSpatial-h5.md`](../output/SpeciesMesoSpatial-h5.md)
  — read that for the full format (species columns, sparse cells, units,
  compression, an h5py example) rather than this recipe, which only covers
  getting a new per-event HDF5 writer wired into the staging/move/manifest
  lifecycle.

## Serial vs MT

This is the one place the HDF5 export pattern must diverge from the
`Run::Merge`-at-end-of-run pattern the counters in
[`create-a-scorer.md`](create-a-scorer.md#serial-vs-mt),
[`create-a-species-counter.md`](create-a-species-counter.md#serial-vs-mt) and
[`mesoscopic-3d-species-counting.md`](mesoscopic-3d-species-counting.md#serial-vs-mt)
all use:

- **Counters (tickets 2-4, 10):** each worker thread under MT owns its own
  in-memory instance (its own `ScoreSpecies`, `MesoSpeciesCounter`,
  `TrackLengthTable`, …), accumulates independently, and the kernel's
  `G4MTRunManager::MergeRun` folds each worker's instance into the master's
  once, at end of run, through a `Run::Merge` override. There is no shared
  mutable state while workers run, so no locking is needed during the run —
  the merge step at the end is the only synchronisation point.
- **HDF5 export:** `MesoSpatialFile::AppendEvent` is guarded by a single
  process-wide `std::mutex` (`Hdf5Mutex()`, a function-local `static`,
  `MesoSpatialFile.cc:22-25`; taken via `std::lock_guard` at the top of
  `AppendEvent`, `MesoSpatialFile.cc:144`) so that under MT, every worker
  thread's call to `AppendEvent` serialises on the *same* file — one worker's
  append fully completes (including the file-existence check, group
  creation and dataset writes) before another's begins.

  This is a different MT strategy, not a missing one, because an HDF5 file
  has no equivalent of the counters' per-worker-instance-then-merge split: a
  `G4Run::Merge` override needs two separate, independently-populated
  objects to fold together, but there is exactly one `SpeciesMesoSpatial.h5`
  staged file per output directory — handing one worker its own file would
  mean either N files to merge by hand after the run (defeating the point of
  one file with `/run<R>/event<E>/...` groups a reader can iterate in one
  open) or, worse, two workers racing to open and write the same `H5::H5File`
  handle, which HDF5's C++ API does not make safe on its own. Locking around
  every append — rather than splitting per worker and merging after — is the
  only option once the output is "one shared file handle", not "one value
  per worker to sum". The cost is that appends serialise across worker
  threads (each `AppendEvent` call briefly blocks every other worker's), but
  since appends happen once per event, after that event's mesoscopic stage
  has already run single-threaded, this does not serialise the actual
  simulation work — only the comparatively cheap file write at the end of
  each event.
- **Serial mode:** there is one thread, so the mutex is never contended; it
  exists purely for the MT case. The staging/move/manifest steps around it
  (`StagingDir`, `MoveStaged`) are unaffected by Serial vs MT either way —
  they only run on the master thread, from `RunAccumulatorMessenger`.

## See also

- [`mesoscopic-3d-species-counting.md`](mesoscopic-3d-species-counting.md) —
  where the data in `SpeciesMesoSpatial.h5` comes from (the mesh-snapshot
  recording that feeds `TimeStepAction::WriteSpatialSnapshots()`).
- [`docs/output/SpeciesMesoSpatial-h5.md`](../output/SpeciesMesoSpatial-h5.md)
  — the full file format: layout, attributes, species columns, sparse
  cells, compression, an h5py reading example.
- [`docs/output/README.md`](../output/README.md) — the project-wide
  staging/dump/prefix/manifest conventions this recipe's file follows
  (`.pending_prechem`, `.pending_meso_spatial`, `EndOfRun_` safety net).
- [`create-a-scorer.md`](create-a-scorer.md#serial-vs-mt) — the
  `Run::Merge`-at-end-of-run pattern this recipe's mutex-based approach
  diverges from, and why.
