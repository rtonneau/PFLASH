# Demonstration data for dnachem-min notebooks

These two small, seeded runs provide the input for the planned notebooks. They
are workflow examples, not converged radiolysis results. Keep generated output
outside Git and set notebook input paths to your own output directories.

## Reproduce the runs

Build `sim.exe` as described in the [project README](../README.md) and
`.claude/geant4-instructions.md`. On Windows, start PowerShell in the project
root and run the following. If using Conda, its `Library/bin` supplies the
HDF5/Qt DLLs. The parent of each `--dir` output must exist.

```powershell
$root = (Get-Location).Path
$scratch = Join-Path $root '.scratch/tests/2026-11-03__read-notebooks-plan.md-and-implement-1-establish-the'
New-Item -ItemType Directory -Force $scratch | Out-Null
if ($env:CONDA_PREFIX) { $env:PATH = "$(Join-Path $env:CONDA_PREFIX 'Library/bin');$env:PATH" }
Push-Location build
try {
  $irtDir = Join-Path $scratch 'irt'
  $irtLog = Join-Path $scratch 'irt.log'
  .\sim.exe ..\notebooks\demo_irt.in --dir $irtDir *> $irtLog
  if ($LASTEXITCODE -ne 0) { throw 'IRT run failed' }
  $mesoDir = Join-Path $scratch 'meso-spatial'
  $mesoLog = Join-Path $scratch 'meso-spatial.log'
  .\sim.exe ..\notebooks\demo_meso_spatial.in --dir $mesoDir *> $mesoLog
  if ($LASTEXITCODE -ne 0) { throw 'Mesoscopic run failed' }
} finally { Pop-Location }
```

Both macros select `PureWater` with no added scavenger, serial `IRT_syn`, a
10 keV electron beam, and two events. `demo_irt.in` sets seeds `12345 67890`,
disables the mesoscopic stage, and runs chemistry to its 1 µs default.
`demo_meso_spatial.in` sets seeds `23456 78901`, enables the mesoscopic stage
and spatial output, hands over at the default 5 ns, and ends at 1 ms. The first
seed in each pair appears in `Manifest.json`. These seeds do not guarantee
byte-identical chemistry results across runs.

The notebook input paths are the two directories under `$scratch`; set them to
your own absolute or relative paths. `--dir` creates the leaf directory, and
each macro's `/run/dumpDataAndReset` writes bare filenames there. Reusing an
output directory overwrites prior data, so choose a fresh one for each run.

## Verify the outputs

The required files are checked on disk, not just in `Manifest.json`. Run this
from the project root after the simulations:

```powershell
$scratch = Join-Path (Get-Location).Path '.scratch/tests/2026-11-03__read-notebooks-plan.md-and-implement-1-establish-the'
$common = @('Manifest.json', 'Species.Txt', 'Species_nt_species.csv',
  'Reactions.Txt', 'Reactions_nt_reactions.csv', 'ReactionsMetadata.csv',
  'PhysicsInteractions.Txt', 'PhysicsInteractions.csv',
  'TrackLengths.Txt', 'TrackLengths.csv',
  'PreChemical_run0_event0.txt', 'PreChemical_run0_event1.txt')
foreach ($run in @('irt', 'meso-spatial')) {
  $dir = Join-Path $scratch $run
  $required = $common
  if ($run -eq 'meso-spatial') {
    $required += @('SpeciesMeso.Txt', 'SpeciesMeso.csv', 'SpeciesMesoSpatial.h5')
  }
  foreach ($name in $required) {
    $file = Join-Path $dir $name
    if (-not (Test-Path $file -PathType Leaf) -or (Get-Item $file).Length -eq 0) {
      throw "Missing or empty: $file"
    }
  }
  $manifest = Get-Content (Join-Path $dir 'Manifest.json') -Raw | ConvertFrom-Json
  if ($manifest.totalEvents -ne 2 -or $manifest.chemistry -ne 'PureWater') {
    throw "Unexpected provenance: $dir"
  }
  Write-Output "${run}: $($required.Count) nonempty files, 2 events"
}
```

The IRT directory supplies physical interaction totals, per-event track
lengths, two pre-chemical event tables, particle-stage species and reaction
curves. The mesoscopic directory supplies the same files plus aggregate
`SpeciesMeso.*` and per-event `SpeciesMesoSpatial.h5`. See
[output layouts](../docs/output/README.md) and the
[HDF5 format](../docs/output/SpeciesMesoSpatial-h5.md) for units and layouts.
The CSV ntuples from Geant4 include metadata lines before their data rows.

## Python and Jupyter

Use Python 3.10+ with `ipykernel`, `pandas`, `numpy`, `h5py`, `matplotlib`, and
an editable [G4Utils](https://github.com/rtonneau/G4Utils) checkout:

```powershell
cd path\to\G4Utils
python -m pip install -e .
python -m pip install ipykernel matplotlib
```

Select that Python environment as the Jupyter kernel. In a cell, check the
imports before opening a dump:

```python
import h5py, matplotlib, numpy, pandas
from g4utils.DnaChem import Dump, Simulation, load_species, load_reactions
from g4utils.HDF5 import SpeciesMesoSpatialFile
```

G4Utils currently reads manifests, species/reaction ntuples and mesoscopic
spatial HDF5. Its `Dump.has_meso` concerns the **spatial HDF5 file**, not
`SpeciesMeso.csv`. Physical-interaction, track-length, pre-chemical and
aggregate mesoscopic readers are not yet part of that API; the first notebook
pass will inspect those formats directly. See `notebooks/PLAN.md` for scope.

## Checked environment and limits

On 2026-11-03, both commands completed with exit code 0 using dnachem-min
commit `322b32b`, `sim.exe` from the `RelWithDebInfo` build, and Geant4
`geant4-11-04-patch-01` (11.4.1). The live Jupyter kernel used Python 3.13.9
at `C:\Users\rtonneau\AppData\Local\anaconda3\python.exe`. Kernel imports of
all packages and G4Utils symbols shown above succeeded. Installed versions:
`pandas 2.3.3`, `numpy 2.3.5`, `h5py 3.15.1`, `matplotlib 3.10.6`,
`ipykernel 6.31.0`. G4Utils was editable from `C:\DEV\python\G4Utils` at
commit `a456290`; installed package metadata reported `0.3.0` while the
checkout's `pyproject.toml` says `0.4.0`. Reinstall the checkout if precise
package metadata is needed.

Each manifest reported 2 events and 20,000 eV deposited energy. The IRT run
produced 12 nonempty required files; the mesoscopic run produced 15, including
a 6.8 MB HDF5 file. Both logs contain the dump and completion markers and no
fatal exception. The mesoscopic log reports `WrongResolution` twice: the
requested 6.25 nm cell is capped to about 15.26 nm on the default 1 mm box.
Read each snapshot's `cellSize_nm` rather than assuming the requested value.
The generated data and logs are ignored under `.scratch/` and are not part of
this repository.

Physical interaction totals are discrete process counts, not track histories.
`TrackLengths.csv` excludes aborted events. Pre-chemical files are selected
water modification and solvated-electron records. `Reactions.*` counts only
particle-stage bimolecular reactions handled by the reaction hook; mesoscopic
reaction firings and scavenger/acid-base reactions are absent. `SpeciesMeso.Txt`
is mean molecule count per event, while `SpeciesMeso.csv` sums counts over
events. Do not combine either with particle-stage G-values without accounting
for stage, time grid, and normalization.
