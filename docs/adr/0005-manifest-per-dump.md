---
status: accepted
---

# One JSON manifest per dump, replacing EnergyDeposit.Txt

Every **Dump** writes a `Manifest.json` (filename and location resolved like every other output file: prefix or subfolder applied) describing what produced the data next to it: `schemaVersion`, the process-wide settings (Chemistry, scavengers, pH, chemistry end time, thread count, Geant4 version), and a `runs[]` array with one entry per `/run/beamOn` folded into the dump (beam particle, energy, position, direction, events, that run's energy deposit, seed), plus totals and the list of data files written. It is written on every dump, including the automatic `EndOfRun_` flush; there is no opt-out switch.

The unit is the dump, not the `/run/beamOn`, because data files only exist at dump time and a dump can span several runs with different beams; a per-`beamOn` manifest would have no data beside it. The beam is captured from the real gun state on a worker thread, carried in the run record, merged in `Run::Merge` and stored per run in `RunAccumulator`, rather than mirrored from `/gun/*` commands on the master, which could drift from the actual gun.

The Geant4-facing collector (`RunManifest`) builds a format-neutral, ordered `DataNode` tree: `RecordRun` builds each run's entry at the end of the run, `Write` builds the top-level entries at dump time. A small, portable, stream-only `JsonWriter` serialises any tree without knowing its keys (no dependency on other project classes, testable without the Geant4 kernel). The collector is therefore the only place listing the manifest's entries: adding or removing one is a single `Add` line and never touches the writer. A per-field struct plus a hand-written writer was the first implementation and was replaced because every entry had to be spelled out in the struct, the collector, the writer and its tests. Containers whose children are all scalars print on one line; others are indented. Numbers carry their unit in the key (`beamEnergy_keV`, `position_um`, `energyDeposit_eV`); directories are stored both as configured and as absolute paths.

`EnergyDeposit.Txt` is removed: the manifest carries `totalEnergyDeposit_eV`, and keeping a second, human-readable copy was rejected. Anything that read `EnergyDeposit.Txt` must read `Manifest.json` instead.

**Consequences**: the manifest records the seed as configured, but output is still not bit-reproducible (see the project notes on non-reproducible chemistry stepping), so it documents a run rather than guaranteeing a replay. A failed manifest write is a warning, not fatal, since the data files are already written. Changing the meaning of an existing field bumps `schemaVersion`; adding fields does not.

## Addendum (2026-09-29): per-event pre-chemical files belong to the dump

The per-event **Pre-chemical file** (`G4DNAChemistryManager::WriteInto`) is written during each event, before the dump that will own it, and so before its prefix or subfolder is known. Each event therefore writes `PreChemical_run<R>_event<E>.txt` into a staging folder, `<outdir>/.pending_prechem/`. At dump time, including the `EndOfRun_` flush, every staged file is moved into the dump as `<prefix>PreChemical_run<R>_event<E>.txt` (or into `<subdir>/`) and listed individually in the manifest's `files`. The dump finds the files by scanning the folder, not through a registry filled by worker threads, so it needs no cross-thread bookkeeping. Event IDs are unique within a run in both Serial and MT, so the name has no thread ID. Leftover files from an aborted earlier process are swept into the first dump. An existing target file is overwritten. Any other move failure is a warning, and the file stays staged and is not listed. An event with no records keeps its empty file, so the number of files equals the number of events.

Each event installs a fresh `G4PhysChemIO::FormattedText` through `SetPhysChemIO` at begin of event. Reusing one writer does not work: Geant4 11.4.1's `CloseFile()` does nothing until the file has a record, so the next event's records land in the previous file and the next `open()` fails.

## Addendum (2026-10-01): wall-clock runtimes

Three runtime fields record how long each dump's data took: `elapsedSinceStart_s` and `elapsedSincePreviousDump_s` at top level, and `wallTime_s` in each `runs[]` entry.

| Key | Span |
|---|---|
| `elapsedSinceStart_s` | start of `main()` (via `RunManifest::MarkProcessStart()`) → this dump |
| `elapsedSincePreviousDump_s` | previous dump (or process start) → this dump |
| `wallTime_s` | master `RunAction::BeginOfRunAction` → master `RunAction::EndOfRunAction` (event loop, chemistry, MT worker merge; run initialization excluded) |

Wall clock only (`std::chrono::steady_clock`), in seconds: CPU time is misleading under MT (summed across workers) and the master's CPU time excludes workers. No total process time is recorded because the `EndOfRun_` flush runs before `theTimer->Stop()`, so the final elapsed time is unknown when any manifest is written. `elapsedSincePreviousDump_s` exceeds the sum of a dump's `runs[].wallTime_s` by the time spent outside the run actions (setup, `/run/initialize`, run initialization such as physics tables on the first `/run/beamOn`, macro commands). `schemaVersion` is unchanged.

## Addendum (2026-10-08): default output directory and results index

With neither `--dir` nor `/run/outputDir`, output goes to `<exeDir>/results` (`OutputDir::SetDefaultDir`, passed in from `sim.cc` so `OutputDir` stays pure). The default is applied lazily, at the first output, so a macro's `/run/outputDir` (PreInit) still replaces it and a run that sets one never creates `results/`; a `/run/outputDir` issued after the default was used is refused.

`<outdir>/Manifest.json` is also a **results index**: after every dump (the `EndOfRun_` flush included) `RunManifest::Write` rewrites it with a top-level `dumps[]` array listing every dump so far (folder, prefix, path of its own manifest, timestamp, events, and per run the id, events, seed and beam). The pure builder is `ResultsIndex` (on `DataNode`, unit-tested); per-dump manifests are unchanged. Collision: a flat dump with an empty prefix writes its per-dump manifest to the same path. Neither may overwrite the other, so that file is written once as the dump's manifest with the `dumps` array added (and rewritten that way by every later dump); it has no `kind` key, whereas a stand-alone index has `kind: "resultsIndex"`. A failed open of the index is a `JustWarning`.
