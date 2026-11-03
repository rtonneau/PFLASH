# Demonstration data for dnachem-min notebooks

These two small, seeded runs provide input for the
[vertical-slice notebook](vertical_slice.ipynb). They are workflow examples,
not converged radiolysis results. Keep generated output outside Git and set
notebook input paths to your own output directories.

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

The notebook's configuration cell defines `IRT_DUMP` and `MESO_DUMP`, defaulting
to `DEV_DIR/GEANT4/SIM/dnachem-min/.scratch/tests/2026-11-03__read-notebooks-plan.md-and-implement-1-establish-the/irt`
and `.../meso-spatial`, respectively.
Edit those variables for your own absolute or relative dump paths. Use
unprefixed dump directories containing their own `Manifest.json`, not the
results-index directory or a filename-prefixed dump. `--dir` creates the leaf
directory, and each macro's `/run/dumpDataAndReset` writes bare filenames
there. Reusing an output directory overwrites prior data, so choose a fresh one.

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

The checked Windows environment is the `GEANT4` Conda environment at
`E:\miniconda3\envs\GEANT4`, selected as the Jupyter kernel. It uses Python
3.13.11 and an editable [G4Utils](https://github.com/rtonneau/G4Utils)
checkout at `D:\DEV\PYTHON\G4Utils` (G4Utils 0.4.0). Checked versions are
pandas 3.0.0, numpy 2.4.2, h5py 3.16.0, matplotlib 3.10.8, ipykernel 6.31.0,
JupyterLab 4.6.4, and Jupyter Server 2.21.1. For other setups, use a compatible
Python environment with `ipykernel`, `pandas`, `numpy`, `h5py`, `matplotlib`,
and an editable G4Utils checkout:

```powershell
conda activate GEANT4
$g4utilsCheckout = Join-Path $env:DEV_DIR 'PYTHON/G4Utils'
python -m pip install -e $g4utilsCheckout
python -m pip install ipykernel matplotlib
python -m ipykernel install --user --name GEANT4 --display-name 'Python (GEANT4)'
```

In Jupyter, select the registered `Python (GEANT4)` kernel. The checked
server's default `python3` kernel also used that environment's interpreter.
In a cell, check the imports before opening a dump:

```python
import h5py, matplotlib, numpy, pandas
from g4utils.DnaChem import Dump, Simulation, load_species, load_reactions
from g4utils.HDF5 import SpeciesMesoSpatialFile
```

Open `vertical_slice.ipynb`, edit `IRT_DUMP` and `MESO_DUMP` in its
configuration cell, then choose **Restart Kernel and Run All Cells**. Check
the printed kernel executable against your intended environment. The default
paths require `DEV_DIR`; missing required inputs raise errors with regeneration
guidance. Missing optional mesoscopic aggregate or spatial output is explained
and skipped. Relative input paths resolve from the kernel's working directory.

G4Utils currently reads manifests, species/reaction ntuples and mesoscopic
spatial HDF5. Its `Dump.has_meso` concerns the **spatial HDF5 file**, not
`SpeciesMeso.csv`. Physical-interaction, track-length, pre-chemical and
aggregate mesoscopic readers are not yet part of that API; this notebook keeps
small direct readers visible. See `PLAN.md` for the later report and library work.

## Checked reproduction and limits

On 2026-11-03, the regenerated manifests report runtime tag
`geant4-11-04-patch-03` (11.4.3), `PureWater`, Serial `IRT_syn`, no added
scavenger, 10 keV electrons, 2 events, and 20,000 eV deposited energy. The
IRT-only run uses seeds `12345 67890`, disables meso, and runs to 1000 ns. The
separate mesoscopic run uses seeds `23456 78901`, hands over at 5 ns, and runs
to 1,000,000 ns. These are distinct seeded simulations, not a continuous
trajectory. The existing `RelWithDebInfo` executable was used; its exact source
revision is not established by the run records.

The `vertical_slice.ipynb` notebook was executed through Jupyter MCP in a fresh
isolated GEANT4 kernel on 2026-11-03. All 10 code cells completed; four plots were
reviewed, raw CSV values and optional missing-file handling were checked. All
required files were present and nonempty: 12 for IRT and 15 for mesoscopic
output. G4Utils editable metadata points to `D:\DEV\PYTHON\G4Utils` at source
revision `a45629041f1d47b1e2d7b9b367aa0a95443a732f`.

Both simulation commands completed with exit code 0; their logs contain dump
and completion markers and no fatal exception. The mesoscopic output includes
a 6.8 MB format-version-2 HDF5 file, with 55 snapshots for each of the two
events. Generated data and logs are ignored under `.scratch/`.

Physical interaction totals are discrete process counts, not track histories.
`TrackLengths.csv` excludes aborted events. Pre-chemical files are selected
water modification and solvated-electron records. `Reactions.*` counts
particle-stage bimolecular reactions handled by the reaction hook. Bulk
acid-base and scavenger channels bypass it, while mobile-particle bimolecular
reactions such as H3O+ + OH- are counted. Mesoscopic reaction firings are not
recorded there. `SpeciesMeso.Txt` is mean molecule count per event, while
`SpeciesMeso.csv` sums counts over events. Do not combine either with
particle-stage G-values without accounting for stage, time grid, and
normalization.

Mesh dimensions are capped at 65,536 pixels per side. On the default 1 mm box,
the requested 6.25 nm cells become 15.258789 nm cells in the snapshots. The logs contain
two `WrongResolution` warnings: the observed O^-1 + H3O^1 reaction radius is
6.3164 nm, at least the actual cell size divided by pi. This is a known
baseline mesh-initialization warning; the cap alone does not explain the
condition, and this does not imply all resolution warnings are benign. Read
each snapshot's `cellSize_nm` rather than assuming the requested value.
