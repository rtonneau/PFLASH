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
