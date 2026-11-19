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
  (e⁻aq + e⁻aq → 2 OH⁻ + H₂). That is not the trigger: the three products are of two species
  (OH⁻, OH⁻, H₂), and the finder keeps one octree per species, so each tree holds at most two of
  them. `all-1` never builds a tree with three coincident molecules: no history comes near the
  memory cap. The trigger is unreachable here, see "Patch 1: reachability" below.
- **Patch 2:** a new molecule must land within the effective reaction radius (about 0.1-0.5 nm)
  of an old one. At about 4000 live molecules in (4 µm)³, that happens about once in 10⁴
  activations. In all 40 histories at 10⁴ Gy/s, 135 reactions per history are between sources,
  but none at the instant of activation.
- **Patch 3:** the stock code fails only when one step goes past a wall by more than the box length
  (4 µm). IRT_syn moves a molecule only when it reacts; even at 0.1 Gy/s no such step occurs.

## Patch 1: reachability

Question: can the trigger of patch 1 (three or more molecules of one species at one exact point)
occur in IRT_syn dose-rate runs? Verdict: **unreachable** with the four built-in Chemistries and
no exogenous scavenger, which is the dose-rate study's setting. Script:
`python test/dose-rate-patches/necessity/coincidence_audit.py --catalog docs/explorer/catalog.json --runs .scratch/dose-rate-patches/necessity`
(exit 0 = unreachable, 1 = reachable, 2 = missing input).

**Source facts** (the script re-checks each line in Geant4 11.4.3 and reports `ok`):

- The finder keeps one octree per species. `G4OctreeFinder.icc:336` builds a tree per list key, the
  track lists are keyed by molecule ID (`G4ITTrackHolder.cc:403`), and a molecule's `ITSubType` is
  its configuration ID (`G4Molecule.hh:127`). OH⁻ and H₂ never share a tree.
- A leaf holds at most 2 entries (`G4Octree.hh:53`) and the depth limit is 100 (`G4Octree.hh:54`).
  A node with more than 2 entries is split (`G4Octree.icc:144`), an entry is copied into every
  child that contains it (`:221`) and the depth counter is shared across siblings (`:226`). A third
  coincident entry in one tree can therefore never be separated.
- `G4DNAMakeReaction.cc:124` puts all products of a reaction at one point.
  `G4DNAScavengerProcess.cc:294` puts the products of a bulk reaction at the reactant's position.
  `G4DNAWaterDissociationDisplacer.cc:229` gives dissociation products random displacements, so
  they do not coincide.
- IRT_syn rebuilds the trees on every interaction step with the products included
  (`G4DNAIndependentReactionTimeStepper.cc:63`, `:75`) and searches partners per partner species
  (`:140`). Products are therefore in the trees, and only same-species coincidence matters.

**Argument**, in four independent checks (each prints in the script output):

1. **Direct events.** In the catalogue (`docs/explorer/catalog.json`, all four Chemistries, 23
   dissociation channels each) no channel, bimolecular reaction or bulk reaction has three products
   of one species. The most is two (OH⁻ + OH⁻, e.g. e⁻aq + e⁻aq → H₂ + 2 OH⁻; 11, 5, 13 and 5 events
   with exactly two, for PureWater, BoscoloChem, Tonneau2025 and Farokhi2023).
2. **Stacking.** Repeated events can pile products at one point: a bulk reaction on a member
   (position kept), or a reaction between two coincident molecules. The script computes the closure
   of the species multiset at one point. With the baseline bulk reactions (pH buffer partners) the
   largest same-species count stays at 2 in all four Chemistries. Adding the O₂-gated bulk
   reactions leaves it at 2 as well. The one route to 3 (OH⁻ ×3, via a distance-zero e⁻aq + O⁻
   reaction) needs two coincident reactive molecules to react at distance 0, which stock Geant4
   never does (the stock query drops distance-zero partners, the secondary defect that patch 1 also
   fixes). It is listed in the script output as an upper bound only.
3. **Recorded logs.** The 80 necessity-run histories (`reactions.csv`, 8 kernel x rate groups of 10)
   hold 61 096 to 61 394 reactions per 10 histories at 0.1 Gy/s and 15 673 to 16 031 at 10⁴ Gy/s,
   and 282 to 331 reactions with two products of one species per group (2014 of the 2476
   such reactions overall are e⁻aq + e⁻aq → H₂ + 2 OH⁻). The number of reactions with three or more products of
   one species is **0**. The logs hold no positions, so this checks the products of each logged
   reaction, not coordinates; stacking across reactions is covered by check 2.
4. **Captured packets.** In the 150 packets of the capture library (9446 molecules), the largest
   number of same-species molecules at one bit-identical position is 1, within a packet and across
   all packets. Packets are never reused within a history, so sources do not stack.

**Caveats:**

- The snapshots (h5) were not scanned for coincident positions (no h5py in the audit run), so the
  logs and the closure stand in for them.
- The verdict holds for the four built-in Chemistries. A new Chemistry with a reaction or channel
  that has three products of one species would reach the trigger directly; the script flags it
  (`direct>=3`) when run on that Chemistry's catalogue.
- The coincident-pair half of patch 1 is a separate matter. Stock drops distance-zero partners, so
  a reactive pair created at one point (22, 12, 24 and 11 events per Chemistry have such a pair
  among their products) cannot react with its coincident partner. This is not the memory blow-up
  and was not measured here.

Consequence: patch 1 stays as a guard, as the conclusion below says, but the study's histories
cannot depend on it at these settings. The "three products at one point" of e⁻aq + e⁻aq is not its
trigger.

## Patch 2 vs dose rate

Question: does patch 2 matter at higher dose rates, where more molecules are alive when a new
track lands? Study `2026-11-18__patch2-necessity-vs-dose-rate` (index: [docs/studies/INDEX.md](../studies/INDEX.md)),
run set `rateSweep`, launched as Study runs with
`pwsh -File test/dose-rate-patches/RunNecessity.ps1 -Config rateSweep -Variants all,all-2 -Reuse -Study <id> -Python <python with numpy and h5py>`.
Same setup as above (same library, 0.1 Gy, `halfBoxUm = 2`, snapshots on), kernels `all` and
`all-2`, 5 rates from 10⁴ to 10⁸ Gy/s, 10 realizations per rate and kernel: 100 histories, all
`complete` (none aborted, none capped). The same seeds and packets run on both kernels.

Measure: new-old contacts at the activation instant (a new molecule inside the reaction radius of
an old one, the case patch 2 fixes), counted on both kernels. Verdict per rate: the defect must
appear in some history (more contacts on `all` than on `all-2`) and G(beam-off) `all − all-2` must
differ (|z| > 3 on a species).

| Rate (Gy/s) | Histories per kernel | Activations | Contacts, all / all-2 | Defect histories | Max abs z, G(beam-off) | Verdict |
|---|---|---|---|---|---|---|
| 10⁴ | 10 | 658 | 0 / 0 | 0/10 | 1.94 (•O) | not demonstrated |
| 10⁵ | 10 | 667 | 0 / 0 | 0/10 | 2.12 (O₂⁻) | not demonstrated |
| 10⁶ | 10 | 648 | 0 / 0 | 0/10 | 1.52 (O⁻) | not demonstrated |
| 10⁷ | 10 | 664 | 0 / 0 | 0/10 | 1.03 (O⁻) | not demonstrated |
| 10⁸ | 10 | 668 | 0 / 0 | 0/10 | 3.20 (O⁻) | not demonstrated |

- **Contacts versus rate:** zero at every rate on both kernels, so the contacts-versus-rate fit
  has nothing to fit ("not available: needs 2 or more rates with contacts > 0" for both
  kernels). No slope can be given. At 0.1 Gy/s the earlier run found 1 event in 10 histories (see
  "Necessity" above); that rate is outside this sweep.
- **G deltas:** pooled over the 5 rates (50 histories per kernel), all − all-2 per 100 eV: H₂O₂
  +0.012 (z 0.68), H₃O⁺ +0.004 (0.05), e⁻aq +0.010 (0.10), •OH −0.008 (−0.07), H• −0.005 (−0.60),
  H₂ −0.002 (−0.14), OH⁻ −0.010 (−0.37), O⁻ +0.003 (0.98), •O −0.007 (−1.92). Per rate, one value
  passes |z| > 3: O⁻ at 10⁸ Gy/s (3.20). It has no contact mechanism behind it (0 contacts on
  both kernels), it is 1 of about 80 species-rate tests, and the pooled z of O⁻ is 0.98. Read it
  as a fluctuation (issue 15: a rebuilt binary gives another realization), not as a patch-2 effect.
  The verdict rule requires a defect history first, so it stays "not demonstrated".
- **Why no contacts (reading, not measured separately):** each history has about 65 activations
  (the 0.1 Gy dose fixes the number of tracks), and a contact needs a new molecule within about
  0.1-0.5 nm of an old one; the earlier estimate at 10⁴ Gy/s was about 1 per 10⁴ activations.
  Within this dose and box, raising the rate to 10⁸ Gy/s did not produce any. A demonstration
  would need more activations per history (larger dose or box), not a higher rate at 0.1 Gy.

Wall-time cost of the demonstration (launcher seconds per history, mean over 10; 4 histories at
once):

| Rate (Gy/s) | all | all-2 |
|---|---|---|
| 10⁴ | 34.2 s | 33.5 s |
| 10⁵ | 31.5 s | 32.3 s |
| 10⁶ | 31.6 s | 32.2 s |
| 10⁷ | 30.1 s | 30.5 s |
| 10⁸ | 26.6 s | 27.2 s |

The full sweep is 3097 s of history time (52 min), about 13 min elapsed at 4 in parallel, far
under the 10 h budget, so nothing was trimmed. One rate on both kernels (20 histories) costs about
11 min of history time, 3 min elapsed. Not included: the kernel builds and the capture library
(reused from the earlier runs, not timed here). Memory peaks at about 65 MB per history. Patch 2
needs no longer runs; what is missing is a trigger, not statistics.

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

## Decision (2026-11-19)

Future kernels drop patches 1 and 3 and keep patch 2: [ADR 0013](../adr/0013-drop-patches-1-and-3.md).
The patch files and fixtures stay on record.
