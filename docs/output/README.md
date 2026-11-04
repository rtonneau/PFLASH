# Output files and directory layout

What `sim` writes, where, and when. File names and sizes below come from real
runs (10 keV e-, `/run/beamOn 2`, Serial, `/scheduler/endTime 1 ms`, default
Chemistry `PureWater`, default 5 ns hand-over, mesoscopic stage on unless stated). Sizes scale with the number of
events and with the chemistry end time, so read them as orders of magnitude.

Detailed semantics (columns, units, commands, failure modes) are in the
`sim-output` skill: [`.claude/skills/sim-output/SKILL.md`](../../.claude/skills/sim-output/SKILL.md).
The spatial snapshot layout has its own page: [`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md); the h5 and root layout of every other product: [`tables-h5-root.md`](tables-h5-root.md).

## When files are written

Nothing is written during `/run/beamOn`, except the staging files below.
Data accumulates across runs and is written by a **dump**:

| Trigger | Where the files go |
|---|---|
| `/run/dumpDataAndReset` | the output directory, bare names |
| `/run/dumpDataAndReset <prefix>` | the output directory, `<prefix>` glued in front of every name (no separator is added) |
| `/run/dumpDataAndResetToDir <subdir>` | `<outdir>/<subdir>/`, bare names; the folder is created |
| none in the macro | one automatic flush at exit with prefix `EndOfRun_` |

The output directory is `--dir <path>` / `/run/outputDir <path>`, else
normally `<exeDir>/results` (e.g. `build/results`), created at the first
output. If executable lookup or lazy default-directory creation fails, the
existing fallback is the current directory. With `--dir`, the default folder
is not created.
A dump writes the data since the previous dump (or process
start) and resets the counters. Reusing a prefix or a subfolder name in one
process is fatal.

## Formats and file names

Each **Output product** is written in the formats chosen for it, as
`<Product>.<ext>` (`Species.csv`, `Reactions.h5`, ...). Formats: `txt`, `csv`,
`h5`, `root`; several at once are allowed. The built-in default is `csv`. Both
commands are PreInit (before `/run/initialize`):

```text
/output/defaultFormat csv h5            # default for every product; 'none' = no file
/output/species/format txt csv h5 root  # one product, replaces the default
```

Products: `species`, `speciesMeso`, `reactions`, `physicsInteractions`,
`trackLengths`, `preChemical`, `speciesMesoSpatial`. `speciesMesoSpatial`
accepts only `h5`/`root` and never follows the default (no file unless its own
command is given). A product skips default formats it does not accept; `h5`
needs an HDF5 build (the registry rejects it otherwise). The chosen lists are
in `Manifest.json` under `outputFormats`. Columns, units and failure modes:
the `sim-output` skill; the h5 and root table layouts:
[`tables-h5-root.md`](tables-h5-root.md).

## Layout examples

Plain dump (`/run/dumpDataAndReset`, default format `csv`):

```text
<outdir>/
  Manifest.json                (dump manifest + results index, see below)
  Species.csv
  Reactions.csv, ReactionsCatalog.csv
  PhysicsInteractions.csv
  TrackLengths.csv
  SpeciesMeso.csv
  PreChemical_run0_event0.csv, PreChemical_run0_event1.csv
  .pending_prechem/            (empty after the dump)
```

With `/output/defaultFormat txt csv h5 root` every product has `.txt`,
`.csv`, `.h5` and `.root` files (one file per table for txt/csv, one file
holding all tables for h5/root); `PreChemical` writes per-event
`PreChemical_run<R>_event<E>.txt/.csv` files plus one `PreChemical.h5` and
`PreChemical.root` per dump.

Prefix dump then subfolder dump, with `/output/speciesMesoSpatial/format h5`:

```text
<outdir>/
  Manifest.json                (results index, lists both dumps)
  pfx_Manifest.json, pfx_Species.csv, ... pfx_SpeciesMesoSpatial.h5
  pfx_PreChemical_run0_event0.csv, pfx_PreChemical_run0_event1.csv
  run02/
    Manifest.json, Species.csv, ... SpeciesMesoSpatial.h5
    PreChemical_run1_event0.csv, PreChemical_run1_event1.csv
  .pending_prechem/            (empty)
  .pending_meso_spatial/       (empty)
```

No dump in the macro: the same file set as the plain dump, every name
prefixed with `EndOfRun_` (`EndOfRun_Manifest.json`, `EndOfRun_Species.csv`, ...),
plus the results index `Manifest.json`.

Mesoscopic stage off (`/chem/meso/enable false`, or SBS): the same trees
without `SpeciesMeso.*`; see [below](#mesoscopic-stage-on-or-off).

The `PreChemical_run<R>_event<E>.<ext>` names carry the run and event index
(both start at 0; the run index keeps counting across dumps, so the second
dump above holds `run1`).

## The results index

`<outdir>/Manifest.json` is also the **results index**, rewritten at every dump
(the `EndOfRun_` flush included): top level `kind: "resultsIndex"`,
`outputDirAbsolute` and `dumps[]`, one entry per dump (`folder`, `prefix`,
`manifest` = relative path of that dump's own manifest, `timestamp`, `events`,
`runs[]` with run, events, seed and beam). Prefixed, subfolder and `EndOfRun_`
dumps keep their own manifest (`pfx_Manifest.json`, `run01/Manifest.json`,
`EndOfRun_Manifest.json`) next to the index. Collision: a flat dump with an
empty prefix has its manifest at the index's path, so the single `Manifest.json`
is that dump's manifest plus a top-level `dumps` array (and no `kind`). Seen in
the real runs: a collided file of about 2.8 KB (3.4 KB with all formats listed).

## File reference

All files are written at a dump unless noted. Sizes are from one real run: 10 keV
e-, `/run/beamOn 2`, Serial, `/scheduler/endTime 1 ms`, `PureWater`, mesoscopic
stage on, so read them as orders of magnitude (they grow with events and end
time). Table columns are in the `sim-output` skill.

| File | Content | Writer | Size: csv / txt / h5 / root |
|---|---|---|---|
| `Manifest.json` | What produced the data beside it: timestamp, Chemistry, scavengers, pH, box, `timeStepModel`, `mesoEnabled`, `outputFormats`, hand-over and end time, mesh settings, run mode, threads, output dir, prefix/subdir, totals, `files`, `runs[]`. In the output directory itself it is also the results index | `src/scoring/RunManifest.cc` via `JsonWriter` (index: `ResultsIndex`) | 2.8 KB (3.4 KB with all formats) |
| `Species.<ext>` | Per (time, species): event count, count sum/mean/standard error, G-value sum/mean/standard error; particle-based stage (the whole run when the mesoscopic stage is off) | `src/scoring/ScoreSpecies.cc` | 49 / 48 / 63 / 51 KB |
| `SpeciesMeso.<ext>` | Same schema for the mesoscopic stage (hand-over time to end time, log grid) | `src/scoring/MesoSpeciesCounter.cc` | 75 / 74 / 96 / 63 KB |
| `Reactions.<ext>` + `ReactionsCatalog.<ext>` | Per (time bin, reaction id): firing-count statistics; the catalogue maps `reactionId` to the `A + B -> C + D` label. Particle-based stage only | `src/scoring/ReactionCounter.cc` | 1.3 KB + 0.5 KB; txt 4.7 KB + 1.1 KB; h5 15 KB and root 36 KB (both tables in one file) |
| `PhysicsInteractions.<ext>` | Per `G4DNA` physics process: count statistics per event, zeros included | `src/scoring/PhysicsInteractionCounter.cc` | 0.3 / 0.7 / 11 / 34 KB |
| `TrackLengths.<ext>` | One row per non-aborted event: primary and secondary-electron track lengths, end reason | `src/scoring/TrackLengthTable.cc` | 0.4 / 0.7 / 12 / 34 KB |
| `PreChemical_run<R>_event<E>.<ext>` (txt, csv) / `PreChemical.<ext>` (h5, root) | Pre-chemical species of the physical stage: parent track, molecule, modification, level, energy, positions | `src/scoring/PreChemicalRecorder.cc`, moved by `PreChemicalFiles::MoveStaged` | csv about 160 KB per event, txt 165 KB per event; h5 400 KB and root 180 KB for 2 events |
| `SpeciesMesoSpatial.<ext>` | Per-event spatial snapshots of the mesoscopic mesh (non-empty cells only). h5: the nested layout of [`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md); root: one long ntuple | `src/scoring/MesoSpatialFile.cc`, `MesoSpatialRootRows.cc` | only with `/output/speciesMesoSpatial/format`; h5 6.9 MB, root 1.7 MB for 2 events (grows with end time) |

The Chemistry decides the species: `Tonneau2025` adds `HO3`.
`Reactions.*` cover the particle-based stage only; the mesoscopic stage
records counts, not which reaction fired.

## Staging folders

Two hidden folders sit in the output directory and are created on demand:

| Folder | Holds | Filled by | Emptied by |
|---|---|---|---|
| `.pending_prechem/` | `PreChemical_run<R>_event<E>.txt/.csv` per finished event; `PreChemical.h5/.root` written at the dump | each event (Serial and MT); the dump for h5/root | every dump moves the files out, in (run, event) order |
| `.pending_meso_spatial/` | `SpeciesMesoSpatial.h5` (appended per event), `SpeciesMesoSpatial.root` (written at the dump) | `TimeStepAction`; the dump for root | every dump moves them out |

Observed during a run (after `/run/beamOn 2`, before the dump): `.pending_prechem/`
held `PreChemical_run0_event0.csv` and `PreChemical_run0_event1.csv`, and with
spatial output `.pending_meso_spatial/SpeciesMesoSpatial.h5` too. After the dump
both folders were still present and empty. A failed move is a
`PreChemicalMoveFailed` / `MesoSpatialMoveFailed` warning and the file stays
staged for the next dump.

## Mesoscopic stage on or off

The mesoscopic stage is on by default under IRT_syn. It is off with
`/chem/meso/enable false` (PreInit) or under SBS
(`/process/chem/TimeStepModel SBS`). Then:

- `SpeciesMeso.<ext>` is not written.
- `Manifest.json` records `mesoEnabled: false`, `timeStepModel` (`IRT_syn` or
  `SBS`), `handOverTime_ns: null`, `voxelSize_nm: null`, `mesoPixels: null`.
- `Species.*` and `Reactions.*` cover the whole chemistry run.
- The default end time is 1 us instead of 1 s (`chemistryEndTime_ns: 1000`);
  the real runs used it unchanged, without a `/scheduler/endTime` line.

## Edge case: end time at or before the hand-over time

With the mesoscopic stage on, `/scheduler/endTime 3 ns` and the default 5 ns
hand-over, the chemistry stops before the mesoscopic stage starts. The dump is
still complete and uses the same file names (re-checked on the current build).
`Species.*` and `Reactions.*` stop at the end time. `SpeciesMeso.<ext>` is
still written but holds only its header (93 B for csv). `Manifest.json` records `chemistryEndTime_ns: 3` and
`handOverTime_ns: 5`. This is different from the mesoscopic stage being off,
where those two files do not exist.

## See also

- [`sim-output` skill](../../.claude/skills/sim-output/SKILL.md): commands, file semantics, prefix and subfolder rules, failure modes.
- [`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md): spatial h5 and root layout, attributes, reading example.
- [`tables-h5-root.md`](tables-h5-root.md): h5 and root layout of the table products.
