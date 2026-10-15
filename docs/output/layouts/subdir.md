# Layout: subfolder

Two ways to move the output away from the default directory: `--dir <path>` (or `/run/outputDir <path>`) sets the output directory (default `<exeDir>/results`, e.g. `build/results`), and `/run/dumpDataAndResetToDir <subdir>` writes one dump into `<outdir>/<subdir>/` with bare names; the subfolder is created. A subfolder name can be used once per process. Staging folders stay in `<outdir>`, not in the subfolder. The trees use `--dir <outdir>` for brevity. The top `Manifest.json` is the results index (`kind: "resultsIndex"`, `dumps[]`, rewritten at every dump); its `dumps[]` entry has `folder: "run01"`, `manifest: "run01/Manifest.json"`, and the subfolder keeps its own full manifest. File meaning: [../README.md](../README.md).

## Default case (mesoscopic stage on)

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndResetToDir run01
```

```text
<outdir>/
  Manifest.json                (results index)
  run01/
    Manifest.json
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
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/spatialOutput true     (before /run/initialize)
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/run/dumpDataAndResetToDir run01
```

```text
<outdir>/
  Manifest.json                (results index)
  run01/
    Manifest.json
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
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/enable false           (before /run/initialize)
/run/beamOn 2
/run/dumpDataAndResetToDir run01
```

```text
<outdir>/
  Manifest.json                (results index)
  run01/
    Manifest.json
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

`SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written when the mesoscopic stage is off (`/chem/meso/enable false`, or SBS). The manifest says `mesoEnabled: false`, `handOverTime_ns: null`, `chemistryEndTime_ns: 1000` (the end time defaults to 1 us, so the `/scheduler/endTime` line is left out), and `Species.*` / `Reactions.*` cover the whole run. See the [README](../README.md#mesoscopic-stage-on-or-off).
