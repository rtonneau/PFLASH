# Output files and directory layout

What `sim` writes, where, and when. File names and sizes below come from real
runs (10 keV e-, `/run/beamOn 2`, Serial, `/scheduler/endTime 1 ms`, default
Chemistry `PureWater`, default 5 ns hand-over, mesoscopic stage on unless stated). Sizes scale with the number of
events and with the chemistry end time, so read them as orders of magnitude.

Detailed semantics (columns, units, commands, failure modes) are in the
`sim-output` skill: [`.claude/skills/sim-output/SKILL.md`](../../.claude/skills/sim-output/SKILL.md).
The HDF5 layout has its own page: [`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md).

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
`<exeDir>/results` (e.g. `build/results`), created at the first output; it is
not the current directory, and with `--dir` the default folder is not created.
A dump writes the data since the previous dump (or process
start) and resets the counters. Reusing a prefix or a subfolder name in one
process is fatal.

## Layout examples

Plain dump (`/run/dumpDataAndReset`):

```text
<outdir>/
  Manifest.json                (dump manifest + results index, see below)
  Species.Txt, Species_nt_species.csv
  Reactions.Txt, Reactions_nt_reactions.csv, ReactionsMetadata.csv
  PhysicsInteractions.Txt, PhysicsInteractions.csv
  TrackLengths.Txt, TrackLengths.csv
  SpeciesMeso.Txt, SpeciesMeso.csv
  PreChemical_run0_event0.txt, PreChemical_run0_event1.txt
  .pending_prechem/            (empty after the dump)
```

Prefix dump then subfolder dump, with `/chem/meso/spatialOutput true`:

```text
<outdir>/
  Manifest.json                (results index, lists both dumps)
  pfx_Manifest.json, pfx_Species.Txt, ... pfx_SpeciesMesoSpatial.h5
  pfx_PreChemical_run0_event0.txt, pfx_PreChemical_run0_event1.txt
  run02/
    Manifest.json, Species.Txt, ... SpeciesMesoSpatial.h5
    PreChemical_run1_event0.txt, PreChemical_run1_event1.txt
  .pending_prechem/            (empty)
  .pending_meso_spatial/       (empty)
```

No dump in the macro: the same file set as the plain dump, every name
prefixed with `EndOfRun_` (`EndOfRun_Manifest.json`, `EndOfRun_Species.Txt`, ...),
plus the results index `Manifest.json`.

Mesoscopic stage off (`/chem/meso/enable false`, or SBS): the same trees
without `SpeciesMeso.Txt` and `SpeciesMeso.csv`; see
[below](#mesoscopic-stage-on-or-off).

The `PreChemical_run<R>_event<E>.txt` names carry the run and event index
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
the real runs: index of 0.6 KB beside the dump manifests, a collided file of
about 1.9 KB.

## File reference

All files are written at a dump unless noted. "Writer" is the source file that
produces the content.

| File | Content | Writer | Appears | Typical size |
|---|---|---|---|---|
| `Manifest.json` | What produced the data beside it: timestamp, Chemistry, scavengers, pH, box, `timeStepModel` (`IRT_syn` or `SBS`), `mesoEnabled`, hand-over and end time, mesh settings, run mode, threads, output dir, prefix/subdir, totals, `files` (the data files of this dump), `runs[]` (beam, energy deposit, seed, wall time). In the output directory itself it is also the results index (`dumps[]`, see above) | `src/scoring/RunManifest.cc` via `JsonWriter` (index: `ResultsIndex`) | every dump, with the same prefix/subfolder as the data | 1.5 to 2 KB; index alone 0.6 KB |
| `Species.Txt` | Species yields (G values) vs. time; up to the hand-over time, or the whole run when the mesoscopic stage is off | `src/scoring/ScoreSpecies.cc` | every dump | 0.5 to 1 KB |
| `Species_nt_species.csv` | Same data as a CSV ntuple: `speciesID,number,nEvent,speciesName,time,sumG,sumG2` | `ScoreSpecies.cc` (Geant4 analysis manager) | every dump | 2 to 3 KB |
| `Reactions.Txt` | Bimolecular reaction firing counts per time bin, particle-based stage only | `src/scoring/ReactionCounter.cc` | every dump | 2 KB |
| `Reactions_nt_reactions.csv` | CSV ntuple `reactionId,time,count` | `ReactionCounter.cc` | every dump | under 1 KB |
| `ReactionsMetadata.csv` | `reactionId,reaction` label map for the ntuple | `ReactionCounter.cc` | every dump | under 1 KB |
| `TrackLengths.Txt` / `.csv` | Per-event primary track length, start/end kinetic energy, end reason (`stopped`/`escaped`/`killed`), secondary-electron lengths and counts; aborted events skipped | `src/scoring/TrackLengthTable.cc` | every dump | about 120 B per event |
| `PhysicsInteractions.Txt` / `.csv` | Firing counts per discrete `G4DNA` physics process (totals, no time bins) | `src/scoring/PhysicsInteractionCounter.cc` | every dump | about 200 B |
| `SpeciesMeso.Txt` | Mean count per event vs. time for the mesoscopic stage (hand-over time to end time, log grid) | `src/scoring/MesoSpeciesCounter.cc` | every dump with the mesoscopic stage on; not written when it is off | about 9 KB at 1 ms end time |
| `SpeciesMeso.csv` | Rows `time_ns,species,count` (counts summed over events) | `MesoSpeciesCounter.cc` | same as above | about 15 KB at 1 ms end time |
| `PreChemical_run<R>_event<E>.txt` | Per-event pre-chemical table: parent track, molecule, ionisation/excitation level, energy, positions | `src/actions/EventAction.cc`, moved by `PreChemicalFiles::MoveStaged` | every dump, one file per event | about 150 KB per 10 keV event |
| `SpeciesMesoSpatial.h5` | Per-event spatial snapshots of the mesoscopic mesh (non-empty cells only) | `src/scoring/MesoSpatialFile.cc` | only with `/chem/meso/spatialOutput true`; one file per dump | about 3 MB per 10 keV event at 1 ms end time (grows with end time) |

The Chemistry decides the species columns: `Tonneau2025` adds `HO3`.
`Reactions.*` cover the particle-based stage only; the mesoscopic stage
records counts, not which reaction fired.

## Staging folders

Two hidden folders sit in the output directory and are created on demand:

| Folder | Holds | Filled by | Emptied by |
|---|---|---|---|
| `.pending_prechem/` | `PreChemical_run<R>_event<E>.txt`, one per finished event | each event (Serial and MT) | every dump moves the files out, in (run, event) order |
| `.pending_meso_spatial/` | `SpeciesMesoSpatial.h5` | `TimeStepAction`, appended per event | every dump moves it out |

Observed during a run (after `/run/beamOn 2`, before the dump): `.pending_prechem/`
held `PreChemical_run0_event0.txt` and `PreChemical_run0_event1.txt`, and with
spatial output `.pending_meso_spatial/SpeciesMesoSpatial.h5` too. After the dump
both folders were still present and empty. A failed move is a
`PreChemicalMoveFailed` / `MesoSpatialMoveFailed` warning and the file stays
staged for the next dump.

## Mesoscopic stage on or off

The mesoscopic stage is on by default under IRT_syn. It is off with
`/chem/meso/enable false` (PreInit) or under SBS
(`/process/chem/TimeStepModel SBS`). Then:

- `SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written.
- `Manifest.json` records `mesoEnabled: false`, `timeStepModel` (`IRT_syn` or
  `SBS`), `handOverTime_ns: null`, `voxelSize_nm: null`, `mesoPixels: null`.
- `Species.*` and `Reactions.*` cover the whole chemistry run, so `Species.Txt`
  is about 1 KB instead of 0.5 KB and `Reactions.Txt` about 4 KB instead of 2 KB.
- The default end time is 1 us instead of 1 s (`chemistryEndTime_ns: 1000`);
  the real runs used it unchanged, without a `/scheduler/endTime` line.

## Edge case: end time at or before the hand-over time

With the mesoscopic stage on, `/scheduler/endTime 3 ns` and the default 5 ns
hand-over, the chemistry stops before the mesoscopic stage starts. The dump is
still complete and uses the same file names (re-checked on the current build).
`Species.*` and `Reactions.*` stop at the end time. `SpeciesMeso.Txt` and
`SpeciesMeso.csv` are still written but contain only their header line (51 B
and 23 B). `Manifest.json` records `chemistryEndTime_ns: 3` and
`handOverTime_ns: 5`. This is different from the mesoscopic stage being off,
where those two files do not exist.

## See also

- [`sim-output` skill](../../.claude/skills/sim-output/SKILL.md): commands, file semantics, prefix and subfolder rules, failure modes.
- [`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md): HDF5 layout, attributes, reading example.
