# Dose-rate patch-necessity suite

Shows, one patch at a time, that each of the three local Geant4 11.4.3 dose-rate
patches is needed: the defect appears on a kernel without the patch, and the patch
removes it.

| Patch | File | Fixture |
|---|---|---|
| 1 | `patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch` | `FinderCoincidentFixture` |
| 2 | `patches/geant4/11.4.3/timed-injection/source-contacts-experimental.patch` | `SourceContactFixture` |
| 3 | `patches/geant4/11.4.3/rebound-fold/rebound-fold.patch` | `ReboundFoldFixture` |

## Kernel variants

`BuildKernels.ps1` builds private kernels under `.scratch/kernels/<variant>/install`
(`kernels.json`: `p1`, `p2`, `p3`, `all`, `all-1`, `all-2`, `all-3`). `stock` is the
shared `${G4_ROOT}` install, which is never modified. `-Verify` checks the applied
patches and the DLL hashes recorded in each `kernel.json`.

## Sufficiency check

```powershell
pwsh -File test/dose-rate-patches/RunSufficiency.ps1 -Patch 3
```

The script builds the suite once per kernel, in `.scratch/dose-rate-patches/suite-<variant>`.
`-DDRP_KERNEL_VARIANT` sets each fixture's `--expect defect|fixed`. The script also checks
that the loaded `G4processes_core.dll` comes from that kernel. A test is green when the
demonstration holds: the defect appears on `stock` and the correct result on `p<N>`.
Fixture output: `RESULT <fixture>.<key>=<value>` lines and `VERDICT`.

## Necessity check (leave-one-out histories)

Prerequisites: the kernels from `BuildKernels.ps1`, MSVC (`vcvars64.bat`), and a Python with
numpy and h5py (`-Python <exe>`; the analysis reads the snapshots).

```powershell
pwsh -File test/dose-rate-patches/RunNecessity.ps1 -Config rate1e4 -Python <python>   # about 10 min
pwsh -File test/dose-rate-patches/RunNecessity.ps1 -Config rate0.1 -Python <python>   # about 1 h at -Parallel 4
```

It builds `examples/dose-rate-study` on `all`, `all-1`, `all-2` and `all-3`, captures one
packet library on `all`, then runs the same plan (configs in `necessity/necessity.json`) on every
variant under `JobLauncher` (memory and wall caps). `-Reuse` keeps finished runs. Output:
`.scratch/dose-rate-patches/necessity/<config>/necessity-results.json`, and one line per patch:
`PASS` when the variant without the patch shows the defect and `all` does not, otherwise
`NOT DEMONSTRATED`.

### Necessity as Study runs (dose-rate sweep)

`rateSweep` (in `necessity.json`): 1e4, 1e5, 1e6, 1e7, 1e8 Gy/s, 10 realizations each, 0.1 Gy, 2 um half box, on
`all` and `all-2` (the config's `variants`), same capture library and seed scheme as the other configs.
`-Study <id>` launches every history through `g4-study.ps1 run` (as `run_study.ps1 -Study` does), so each run has a
provenance stamp under `${STUDIES_ROOT}/PFLASH/<study>/runs/<runSet>/<variant>-<run id>/`
(`stamp.json`, `stdout.log`, `output/`; a `+` of ids such as `r1e+08_000` is dropped in the Study run id).
The JobLauncher memory and wall caps still apply (the launcher is the wrapper's child). The history executable
is the run's `--target`: it is snapshotted and hashed beside the launcher, the launcher runs the snapshot, and
the stamp's Geant4 block is the variant kernel. `g4-study run` refuses an Unmarked kernel, so mark each
`.scratch/kernels/<variant>/install` with `g4-study.ps1 mark` before the runs. Without `-Study` nothing changes.

```powershell
pwsh -File test/dose-rate-patches/RunNecessity.ps1 -Config rateSweep -Variants all,all-2 -Reuse -Study <study-id> -RunSet smoke -Python <python>
```

`-RunSet <name>` names the run set (default: the config name). A `runSets.<name>` entry of the config
overrides study keys for that run set and gets its own plan directory (`<config>-<name>`): `smoke` is one
realization at 1e8 Gy/s. `-Variants all,all-2` also works as one token (split on commas). The analysis reads the
Study runs through links in `.scratch/dose-rate-patches/necessity/<config[-runSet]>/runs/`; with a variant
subset it reports the missing variants and the other patches as `NOT DEMONSTRATED`. A succeeded Study run is
never rerun; a failed one is rerun (the wrapper keeps the earlier attempt as `<id>.prev-<time>`). Study of the
sweep: `2026-11-18__patch2-necessity-vs-dose-rate`.

## Whole suite and report

```powershell
pwsh -File test/dose-rate-patches/RunAll.ps1 -Reuse -Python <python>
```

Runs kernels, sufficiency 1-3, necessity for every config, then `necessity/report.py`, which
writes `patch-necessity-report.md` (+ `.json`) in the newest `.scratch/tests/` session (or
`-Report <path>`). Exit 0 only when every check holds. Current result (2026-11-18): sufficiency
passes for all three patches; necessity holds for patch 2 only (one event at 0.1 Gy/s). See
`docs/dose-rate-pulse-study/patch-necessity.md`.
