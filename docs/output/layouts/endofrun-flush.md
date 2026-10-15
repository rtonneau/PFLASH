# Layout: `EndOfRun_` flush

A macro with no `/run/dumpDataAndReset` or `/run/dumpDataAndResetToDir` still writes its data: `sim` flushes once at exit with the fixed prefix `EndOfRun_`, into the output directory (`--dir`; without it, `<exeDir>/results`, e.g. `build/results`). The prefix is glued on like any other. The trees use `--dir <outdir>` for brevity. `<outdir>/Manifest.json` is the results index (`kind: "resultsIndex"`, `dumps[]`), rewritten at every dump; here it is separate from the dump's own manifest, which keeps its prefix or subfolder. Its `dumps[]` entry has `prefix: "EndOfRun_"`, `manifest: "EndOfRun_Manifest.json"`. File meaning: [../README.md](../README.md).

## Default case (mesoscopic stage on)

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
(no dump command)
```

```text
<outdir>/
  Manifest.json                (results index)
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_TrackLengths.Txt
  EndOfRun_TrackLengths.csv
  EndOfRun_SpeciesMeso.Txt
  EndOfRun_SpeciesMeso.csv
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
  .pending_prechem/            (empty)
```

## Spatial output

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/spatialOutput true     (before /run/initialize)
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
(no dump command)
```

```text
<outdir>/
  Manifest.json                (results index)
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_TrackLengths.Txt
  EndOfRun_TrackLengths.csv
  EndOfRun_SpeciesMeso.Txt
  EndOfRun_SpeciesMeso.csv
  EndOfRun_SpeciesMesoSpatial.h5
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
  .pending_prechem/            (empty)
  .pending_meso_spatial/       (empty)
```

## Mesoscopic stage off

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/enable false           (before /run/initialize)
/run/beamOn 2
(no dump command)
```

```text
<outdir>/
  Manifest.json                (results index)
  EndOfRun_Manifest.json
  EndOfRun_Species.Txt
  EndOfRun_Species_nt_species.csv
  EndOfRun_Reactions.Txt
  EndOfRun_Reactions_nt_reactions.csv
  EndOfRun_ReactionsMetadata.csv
  EndOfRun_PhysicsInteractions.Txt
  EndOfRun_PhysicsInteractions.csv
  EndOfRun_TrackLengths.Txt
  EndOfRun_TrackLengths.csv
  EndOfRun_PreChemical_run0_event0.txt
  EndOfRun_...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written when the mesoscopic stage is off (`/chem/meso/enable false`, or SBS). The manifest says `mesoEnabled: false`, `handOverTime_ns: null`, `chemistryEndTime_ns: 1000` (the end time defaults to 1 us, so the `/scheduler/endTime` line is left out), and `Species.*` / `Reactions.*` cover the whole run. See the [README](../README.md#mesoscopic-stage-on-or-off).
