# Layout: filename prefix

`/run/dumpDataAndReset <prefix>` writes into the output directory and glues the prefix in front of every file name, without a separator (`pfx_` below includes its underscore). The staging folders stay unprefixed. A prefix can be used once per process. Without `--dir` the output directory is `<exeDir>/results` (e.g. `build/results`); the trees below use `--dir <outdir>` for brevity and are the same there. `<outdir>/Manifest.json` is the results index (`kind: "resultsIndex"`, `dumps[]`), rewritten at every dump; here it is separate from the dump's own manifest, which keeps its prefix or subfolder. Its `dumps[]` entry has `folder: ""`, `prefix: "pfx_"`, `manifest: "pfx_Manifest.json"`. File meaning: [../README.md](../README.md).

## Default case (mesoscopic stage on)

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  Manifest.json                (results index)
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_TrackLengths.Txt
  pfx_TrackLengths.csv
  pfx_SpeciesMeso.Txt
  pfx_SpeciesMeso.csv
  pfx_PreChemical_run0_event0.txt
  pfx_...
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
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  Manifest.json                (results index)
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_TrackLengths.Txt
  pfx_TrackLengths.csv
  pfx_SpeciesMeso.Txt
  pfx_SpeciesMeso.csv
  pfx_SpeciesMesoSpatial.h5
  pfx_PreChemical_run0_event0.txt
  pfx_...
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
/run/dumpDataAndReset pfx_
```

```text
<outdir>/
  Manifest.json                (results index)
  pfx_Manifest.json
  pfx_Species.Txt
  pfx_Species_nt_species.csv
  pfx_Reactions.Txt
  pfx_Reactions_nt_reactions.csv
  pfx_ReactionsMetadata.csv
  pfx_PhysicsInteractions.Txt
  pfx_PhysicsInteractions.csv
  pfx_TrackLengths.Txt
  pfx_TrackLengths.csv
  pfx_PreChemical_run0_event0.txt
  pfx_...
  .pending_prechem/            (empty)
```

`SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written when the mesoscopic stage is off (`/chem/meso/enable false`, or SBS). The manifest says `mesoEnabled: false`, `handOverTime_ns: null`, `chemistryEndTime_ns: 1000` (the end time defaults to 1 us, so the `/scheduler/endTime` line is left out), and `Species.*` / `Reactions.*` cover the whole run. See the [README](../README.md#mesoscopic-stage-on-or-off).
