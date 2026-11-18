# Are the three dose-rate patches needed? (2026-11-17/18)

The dose-rate study runs on Geant4 11.4.3 with three local patches (issues 1-3 of
[geant4-issues.md](geant4-issues.md)). This note reports a test suite that checks each patch on
its own, in two ways:

- **Sufficiency:** a small fixture triggers the defect directly. It must show the defect on a
  kernel without the patch and the correct result with it.
- **Necessity:** real `examples/dose-rate-study` histories run on the full kernel (`all`) and on
  the kernel that lacks one patch (`all-1`, `all-2`, `all-3`). The question is whether the defect
  appears in an actual study history.

Code: `test/dose-rate-patches/` (README there). One command runs everything and writes a report:
`pwsh -File test/dose-rate-patches/RunAll.ps1 -Reuse -Python <python with numpy and h5py>`.

## Kernel variants

`BuildKernels.ps1` builds private copies of Geant4 11.4.3 under `.scratch/kernels/<variant>`.
The shared install (`${G4_ROOT}`, `stock`) is never modified.

| Variant | Patches |
|---|---|
| `stock` | none (shared install) |
| `p1`, `p2`, `p3` | only patch 1, 2 or 3 |
| `all` | 1 + 2 + 3 |
| `all-1`, `all-2`, `all-3` | all but one |

| # | Patch | File |
|---|---|---|
| 1 | octree finder, coincident molecules | `molecular-finder/coincident-neighbors.patch` |
| 2 | IRT_syn contact for a new source next to an old molecule | `timed-injection/source-contacts-experimental.patch` |
| 3 | rebound fold in `G4ChemReboundTransportation` | `rebound-fold/rebound-fold.patch` (split out of `meso-boundaries-experimental.patch`) |

`examples/dose-rate-study` builds on a kernel without patch 2: CMake detects
`RegisterSourceActivation` and, if it is missing, injects sources as plain delayed tracks. The
ledger records this as `sourceActivation`.

## Sufficiency: every patch fixes its defect

Each fixture runs on `stock` (defect expected) and on `p<N>` (fixed expected). Every run also
checks that `G4processes_core.dll` was loaded from that kernel.

| Patch | Fixture | stock | patched |
|---|---|---|---|
| 1 | 3 coincident molecules + 6 neighbours, build the octree, query | memory cap (256 MiB) hit after 0.5 s while building | builds (1.7 MiB peak); query returns the 3 coincident molecules and 2 partners |
| 2 | 20 histories: old e⁻aq at 1 ps, new e⁻aq 0.01 nm away at 1.001 ps (P = 1) | 0/20 new-old reactions at activation | 20/20 |
| 3 | 100 000 raw coordinates through `calculateNextCoordinate` in a [-2, 2] µm box | 2750 outside the box, 3763 differing from the exact fold | 0 and 0 |

Control in fixture 2: an old-old pair inside its reaction radius at the start time reacts in 20/20
histories on both kernels, so the start-time contact works without the patch.

## Necessity: the study histories rarely reach the defects

Setup: one capture library of 150 packets (500 keV e⁻, 4 µm box) made on `all`. One plan per dose
rate, so every kernel runs the same configs, seeds and packets. 0.1 Gy, `halfBoxUm = 2`,
10 realizations per kernel, snapshots on. Each history runs under a 3 GiB memory cap and a 1 h
wall cap. Two dose rates: 10⁴ Gy/s (10 µs of beam, about 27 s of wall time per history) and
0.1 Gy/s (1 s of beam, about 6 min).

| Patch | Measure | 10⁴ Gy/s: without / with | 0.1 Gy/s: without / with | Shown? |
|---|---|---|---|---|
| 1 | histories aborted (memory cap or crash) | 0/10 / 0/10 | 0/10 / 0/10 | no |
| 2 | new-old reactions at an activation instant | 0 / 0 | 0 / 1 | barely (1 event in 10 histories) |
| 3 | molecules outside the box (snapshots, final positions) | 0 / 0 | 0 / 0 | no |

What the histories contain:

- **Patch 1:** each 10⁴ Gy/s history has 16 to 31 reactions with three products at one point
  (e⁻aq + e⁻aq → 2 OH⁻ + H₂), the trigger of the defect. Even so, `all-1` never builds the octree
  with three or more coincident molecules in it: no history comes near the memory cap.
- **Patch 2:** a new molecule must land within the effective reaction radius (about 0.1-0.5 nm)
  of an old one. At about 4000 live molecules in (4 µm)³, that happens about once in 10⁴
  activations. In all 40 histories at 10⁴ Gy/s, 135 reactions per history are between sources,
  but none at the instant of activation.
- **Patch 3:** the stock code fails only when one step goes past a wall by more than the box length
  (4 µm). IRT_syn moves a molecule only when it reacts; even at 0.1 Gy/s no such step occurs.

## G(beam-off) differences (report only)

G at beam-off per species, mean of 10 realizations. Differences are shown as `all − variant`.
Every |z| is below 2 at 10⁴ Gy/s and below 1.5 at 0.1 Gy/s: no patch changes the study's G values
at these settings. Full tables: the generated report
(`.scratch/tests/2026-11-17__dose-rate-patch-tests/patch-necessity-report.md`).

| Species | 10⁴ Gy/s, all | − all-2 | − all-3 | 0.1 Gy/s, all | − all-2 | − all-3 |
|---|---|---|---|---|---|---|
| e⁻aq | 2.532 | −0.016 | −0.008 | 0.107 | −0.001 | +0.002 |
| •OH | 2.675 | −0.012 | +0.006 | 0.245 | +0.001 | +0.014 |
| H₂O₂ | 0.513 | +0.009 | −0.014 | 0.969 | +0.007 | −0.002 |
| H₂ | 0.516 | +0.014 | −0.000 | 1.177 | +0.018 | +0.004 |
| H• | 0.685 | +0.009 | +0.016 | 0.124 | +0.001 | +0.003 |

## Conclusion and caveats

- Each patch fixes a real defect, shown on purpose-built inputs.
- At 0.1 Gy, 10⁴ and 0.1 Gy/s, in a 4 µm box, the study histories do not depend on them. Patch 2
  acted once in 10 low-rate histories. Patches 1 and 3 never did. The G values agree within
  statistics.
- The patches stay in place as guards, because each defect is fatal or silent when it does
  occur: a memory blow-up, a missed contact, or a molecule outside the box. Larger doses, denser
  packets, longer late steps (lower rates, longer post-beam relaxation) or other boxes may reach
  them; those settings were not run.
- 10 realizations per kernel and 0.1 Gy only. The z values test the means, not rare events.
- The histories are not reproducible bit for bit, even for one binary (issue 15), so kernels are
  compared statistically, not run by run.
