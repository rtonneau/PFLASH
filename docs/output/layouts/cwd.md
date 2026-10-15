# Layout: default directory

No `--dir` and no `/run/outputDir`: output goes to `<exeDir>/results` (for the usual build, `build/results`), with bare file names; the folder is created at the first output. It is not the current directory. One `/run/dumpDataAndReset` writes everything. With `--dir <path>` the same files land in `<path>` and `<exeDir>/results` is not created (checked). File meaning: [../README.md](../README.md).

The `Manifest.json` here is special. It is the **results index** (`kind: "resultsIndex"`, `dumps[]`), rewritten at every dump. A flat dump with an empty prefix has its own manifest at the same path, so the two collide into one file: the dump's manifest (timestamp, Chemistry, `files`, `runs[]`, ...) plus a top-level `dumps` array, without `kind`. Its single `dumps[]` entry reads `folder: ""`, `prefix: ""`, `manifest: "Manifest.json"`.

## Default case (mesoscopic stage on)

```bash
cd build
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<exeDir>/results/
  Manifest.json                (dump manifest + dumps[] index, see above)
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  TrackLengths.Txt
  TrackLengths.csv
  SpeciesMeso.Txt
  SpeciesMeso.csv
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
```

## Spatial output

```bash
cd build
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/spatialOutput true     (before /run/initialize)
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<exeDir>/results/
  Manifest.json                (dump manifest + dumps[] index)
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  TrackLengths.Txt
  TrackLengths.csv
  SpeciesMeso.Txt
  SpeciesMeso.csv
  SpeciesMesoSpatial.h5
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
  .pending_meso_spatial/       (empty)
```

## Mesoscopic stage off

```bash
cd build
./sim <macro>.in
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`). The `/scheduler/endTime` line is left out: with the mesoscopic stage off the end time defaults to 1 us (a 1 ms end time would also work, it only runs longer):

```text
/chem/meso/enable false           (before /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset
```

```text
<exeDir>/results/
  Manifest.json                (dump manifest + dumps[] index)
  Species.Txt
  Species_nt_species.csv
  Reactions.Txt
  Reactions_nt_reactions.csv
  ReactionsMetadata.csv
  PhysicsInteractions.Txt
  PhysicsInteractions.csv
  TrackLengths.Txt
  TrackLengths.csv
  PreChemical_run0_event0.txt
  ...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written, and the manifest says `mesoEnabled: false`, `chemistryModel: "IRT_syn"`, `handOverTime_ns: null`, `chemistryEndTime_ns: 1000`. `Species.*` and `Reactions.*` then cover the whole run (Species.Txt about 1 KB instead of 0.5 KB). Same result under SBS (`/process/chem/TimeStepModel SBS`), with `timeStepModel: "SBS"`; see the [README](../README.md#mesoscopic-stage-on-or-off).
