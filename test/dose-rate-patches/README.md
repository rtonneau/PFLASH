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

## Whole suite and report

```powershell
pwsh -File test/dose-rate-patches/RunAll.ps1 -Reuse -Python <python>
```

Runs kernels, sufficiency 1-3, necessity for every config, then `necessity/report.py`, which
writes `patch-necessity-report.md` (+ `.json`) in the newest `.scratch/tests/` session (or
`-Report <path>`). Exit 0 only when every check holds. Current result (2026-11-18): sufficiency
passes for all three patches; necessity holds for patch 2 only (one event at 0.1 Gy/s). See
`docs/dose-rate-pulse-study/patch-necessity.md`.
