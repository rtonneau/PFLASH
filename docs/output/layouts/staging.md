# Layout: staging folders

Two hidden folders in the output directory hold files between the end of an event and the next dump: `.pending_prechem/` (one `PreChemical_run<R>_event<E>.txt` per event) and `.pending_meso_spatial/` (`SpeciesMesoSpatial.h5`, only with `/chem/meso/spatialOutput true`). Each dump moves the files out and leaves both folders in place, empty. The output directory is `--dir`, else `<exeDir>/results` (e.g. `build/results`). File meaning: [../README.md](../README.md).

Each tree below is the listing taken by `/control/shell dir /s /b <outdir>` right after `/run/beamOn 2` (before the dump; the real runs used the default directory, `build
esults`), then the tree after `/run/dumpDataAndReset`.

## Default case (mesoscopic stage on)

```bash
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/scheduler/endTime 1 ms            (after /run/initialize)
/run/beamOn 2
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
```

After the dump (a flat dump with an empty prefix: the dump's manifest and the results index are one `Manifest.json`):

```text
<outdir>/
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
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
  .pending_meso_spatial/
    SpeciesMesoSpatial.h5
```

After the dump (a flat dump with an empty prefix: the dump's manifest and the results index are one `Manifest.json`):

```text
<outdir>/
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
./sim <macro>.in --dir <outdir>
```

Macro lines that differ per case (the rest is `/gun/particle e-`, `/gun/energy 10 keV`):

```text
/chem/meso/enable false           (before /run/initialize)
/run/beamOn 2
/control/shell dir /s /b <outdir>
/run/dumpDataAndReset
```

After `/run/beamOn 2`, before the dump:

```text
<outdir>/
  .pending_prechem/
    PreChemical_run0_event0.txt
    PreChemical_run0_event1.txt
```

After the dump (a flat dump with an empty prefix: the dump's manifest and the results index are one `Manifest.json`):

```text
<outdir>/
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

`SpeciesMeso.Txt` and `SpeciesMeso.csv` are not written when the mesoscopic stage is off (`/chem/meso/enable false`, or SBS). The manifest says `mesoEnabled: false`, `handOverTime_ns: null`, `chemistryEndTime_ns: 1000` (the end time defaults to 1 us, so the `/scheduler/endTime` line is left out), and `Species.*` / `Reactions.*` cover the whole run. See the [README](../README.md#mesoscopic-stage-on-or-off).
