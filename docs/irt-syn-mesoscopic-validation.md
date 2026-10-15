# IRT_syn + mesoscopic chemistry: validation report

Validation of the chemistry model of [[0006-irt-syn-and-mesoscopic-chemistry]] (particle-based IRT_syn stage up to a 5 ns hand-over, then the Gillespie mesoscopic stage on a cell mesh up to the end time). Everything here is for a 10 keV electron in the 1 mm water box, `PureWater` Chemistry, seeds `12345 67890`, Ritchie1994 solvation. Numbers come from the build at `baf4b29` (includes the two fixes listed in section 3) unless a section says otherwise.

**Summary.** IRT_syn + mesoscopic is the model of record. The SBS reference (`validation/reference/sbs_{water,o2}`) is a regression baseline, not ground truth: the two models disagree at 1 us by up to +51 % (H2O2, water), and the H2O2 difference already exists in IRT_syn alone (e_aq and OH differences are mostly added by the mesoscopic stage). The comparison is documented, not gated. Serial and `--threads 4` agree within the gate.

Tool: `analysis/compare_reference.py <reference_dir> <new_dir> [--tol 0.10] [--plot out.png]` (conda env `GEANT4_py311`; exit 0 within tolerance, 1 above, 2 on bad input). G is per 100 eV, taken at the latest time <= 1 us; for the new model, times after the hand-over come from `SpeciesMeso.csv` (pooled counts / total energy deposit).

## 1. Comparison with SBS at 1 us (documentation, not a gate)

Same macros (`macro/validate_water.in`, `macro/validate_o2.in`, end time 1 us), run with `--threads 10`. Dumps: `.scratch/tests/2026-10-02__irt-syn-mesoscopic/fix-validate_{water,o2}`. Default `--tol 0.10`, so exit 1 for both pairs is expected.

Water (N = 40 both; SBS statistical error 1.6 to 2.6 %, see `validation/reference/sbs_water/README.md`):

| Species | G SBS | G IRT_syn+meso | rel. diff |
|---|---|---|---|
| e_aq | 1.2445 | 1.5557 | +25.0 % |
| OH | 1.5973 | 1.9352 | +21.2 % |
| H2O2 | 0.4293 | 0.6488 | +51.1 % |
| H2 | 0.7130 | 0.6993 | -1.9 % |

O2 21 % (SBS N = 1500, new N = 1500; SBS statistical error 0.3 to 2.3 %):

| Species | G SBS | G IRT_syn+meso | rel. diff |
|---|---|---|---|
| e_aq | 0.0131 | 0.0143 | +9.0 % |
| OH | 1.9988 | 1.9648 | -1.7 % |
| H2O2 | 0.5387 | 0.7374 | +36.9 % |
| H2 | 0.4518 | 0.5916 | +30.9 % |

Regression of the current SBS mode against these same references: `docs/sbs-regression.md` (it agrees with them once SBS keeps every reaction type 0; the SBS column above is type 0, the IRT_syn columns use type 1 for the partially diffusion-controlled pairs).

Plots (solid: new model, dashed: SBS): `docs/irt-syn-meso-water.png`, `docs/irt-syn-meso-o2.png`.

Wall times: water, SBS 104.6 s vs new 28.5 s (ratio 0.27); O2, SBS 5217.8 s vs new 1184.5 s (ratio 0.23). Both are MT x10 runs; the SBS timings are from the reference manifests.

## 2. Which model is right: diagnostics D1 to D3

IRT_syn and SBS disagree on their own, with an identical reaction table, and the mesoscopic stage adds a further surplus on top. Water, N = 40 each, SBS as reference. These runs were made before the two fixes of section 3 (the fixes do not affect D1 to D3 except where noted):

| Run | e_aq | OH | H2O2 | H2 |
|---|---|---|---|---|
| Full model, first attempt (pre-fix build) | +27.2 % | +22.4 % | +50.8 % | -2.5 % |
| D1: IRT_syn only (hand-over 2 us > end time) | +16.2 % | +7.3 % | +42.7 % | +2.6 % |
| D2: 400 um box (6.1 nm cells) | +31.2 % | +23.4 % | +52.9 % | -4.6 % |
| D3: IRT_syn only, all reactions type 0 | +18.7 % | +6.5 % | +45.8 % | +1.3 % |
| Full model, post-fix (section 1) | +25.0 % | +21.2 % | +51.1 % | -1.9 % |

O2 pair, D1 repeated: IRT_syn only (`07b-d1o2`, N = 300; pre-fix build, but with no hand-over the fixed code paths are not reached) vs SBS: e_aq +4.0 %, OH -9.4 %, H2O2 +29.8 %, H2 +32.8 %. Compared with the full O2 model (+9.0 / -1.7 / +36.9 / +30.9 %), the mesoscopic stage adds little in O2 water.

Reading:
- IRT_syn alone already differs from SBS in H2O2 (+43 % water, +30 % O2) and, in O2 water, in H2 (+33 %). The reaction table is the same, so this is a model difference (time-step method and reaction-scheduling), not a rate difference. D3 (all reactions type 0) leaves it unchanged, so the diffusion-controlled classification is not the cause.
- The mesoscopic stage in pure water adds about +9 points on e_aq (+16.2 to +25.0 %) and +14 points on OH (+7.3 to +21.2 %) over IRT_syn alone. D2 (smaller box, 6.1 nm cells) did not remove it, but the cell-size test was inconclusive (D2 also changes the box, the particle-stage geometry and the pool).
- No test here shows which model handles e_aq + H2O2 and the related reactions correctly.

Decision (recorded in ADR 0006): IRT_syn + mesoscopic is the model of record. SBS is kept only as a regression baseline.

### Open question

1. Cause of the water mesoscopic surplus (about +9 points e_aq, +14 points OH over IRT_syn alone). Not established.
2. Cause of the IRT_syn vs SBS difference in H2O2 and H2. Not established. An analytic pair test (two species, a known diffusion-controlled rate, compared with both models' decay) could decide which model treats e_aq + H2O2 and related reactions correctly. This is a possible follow-up, not done here.
3. In the 1 s O2 run (section 6), O2^- halves and H2O2 rises from 0.82 to 1.46 per 100 eV between about 0.13 s and 1 s. A rough well-mixed estimate (about 230 O2^- per event in 1 uL, about 4e-16 M) gives negligible second-order reaction in that time, so this needs an explanation (physics or mesh effect). It was not investigated.

## 3. Fixes found during validation

| Commit | Defect | Effect | Before -> after |
|---|---|---|---|
| `e0a5b88` | `G4DNAEventScheduler::Voxelizing` left radiolytic O2 tracks alive at the hand-over (the scavenger material holds O2, so the voxelizer skips them). IRT_syn paired them with killed tracks and `FindReaction` looped forever. They are now merged into the bulk pool and killed before voxelizing. | O2 run hung on one worker (1495 of 1500 events, over 50 min, about 1 hang per 1500 O2 events). 271 of 300 O2 events had 1 to 6 such tracks. | hang -> `validate_o2.in` completes, 1500 events, 1184.5 s; the hang did not reproduce |
| `baf4b29` | `G4DNAEventScheduler::RecordTime` runs only after reaction steps, so late `SpeciesMeso` record times kept stale counts. `SpeciesMeso` is now recorded by our mesh action after every Gillespie step. | With bulk O2, e_aq stopped decaying from about 800 ns. | O2 e_aq vs SBS at 1 us: +159 % -> +9 % |

## 4. Serial vs `--threads 4` (gate)

Method (batch means): `macro/validate_water.in` conditions, 5 blocks of 40 events in one process (`/run/beamOn 40` then `/run/dumpDataAndResetToDir blkN`, same seeds), once Serial and once with `--threads 4`, on the post-fix build. G(1 us) of each block is computed; per species the mean and standard error of the 5 block values; the relative error of the difference is sqrt(SE_serial^2 + SE_mt^2) / mean_serial. The gate tolerance is 3 x the largest of these (1.11 %), i.e. `--tol 0.0332`. Then `compare_reference.py` is run on the pooled 200-event dumps (serial as the reference).

| Species | G Serial (+/- SE) | G MT4 (+/- SE) | rel. diff | rel. SE of diff |
|---|---|---|---|---|
| e_aq | 1.5956 +/- 0.0137 | 1.5668 +/- 0.0112 | -1.80 % | 1.11 % |
| OH | 1.9451 +/- 0.0143 | 1.9120 +/- 0.0109 | -1.70 % | 0.92 % |
| H2O2 | 0.6431 +/- 0.0041 | 0.6626 +/- 0.0036 | +3.04 % | 0.85 % |
| H2 | 0.6908 +/- 0.0034 | 0.6978 +/- 0.0031 | +1.02 % | 0.67 % |

Result: max |rel. diff| 3.04 % < tol 3.32 %: PASS, exit 0. Caveat: the H2O2 difference is 3.6 times its standard error (only 5 blocks, so a wide t-distribution; 4 species tested). It passes the gate as defined and is not evidence of a bias, but a larger run could be used to check. Wall time: Serial 1204.3 s, MT4 268.0 s for 200 events each (ratio 0.22). Logs and dumps: `.scratch/tests/2026-10-02__irt-syn-mesoscopic/07c-{serial,mt4}*`.

## 5. Hand-over time: 5 ns vs 20 ns

Measured on the first-attempt (pre-fix) build, `validate_water.in` with `/chem/meso/handOverTime 20 ns` vs the default 5 ns, N = 40 each, 1 mm box, 15.26 nm cells. This is a measurement, not a gate, and it was not repeated on the post-fix build.

| Species | G (5 ns) | G (20 ns) | rel. diff |
|---|---|---|---|
| e_aq | 1.5830 | 1.5542 | -1.8 % |
| OH | 1.9547 | 1.8827 | -3.7 % |
| H2O2 | 0.6475 | 0.6705 | +3.6 % |
| H2 | 0.6950 | 0.7160 | +3.0 % |

The differences are of the same size as the statistical error of N = 40 (1.6 to 2.6 % in the SBS reference at the same N). The 5 ns hand-over is therefore not distinguishable from 20 ns at this precision, so the coarser cell does not show up as a hand-over sensitivity. This does not explain the water surplus of section 2.

## 6. Long runs to 1 s

One run per pair, `/scheduler/endTime 1 s`, `--threads 10`, **N = 40 events**, seeds as above, hand-over 5 ns. The full 1 s was feasible (O2 took 36 minutes), so no 100 ms fallback was needed. Macros `07c-long_{water,o2}.in`, dumps in `.scratch/tests/2026-10-02__irt-syn-mesoscopic/07c-long_{water,o2}`. Both exit 0, success markers present, no `EEEE`; the only `WWWW` is one `WrongResolution` per event (documented, benign).

Plots: `docs/irt-syn-meso-long-water.png`, `docs/irt-syn-meso-long-o2.png` (G(t) from the earliest recorded time to 1 s; SBS to 1 us dashed; vertical lines at the 5 ns hand-over and 1 us). G at 5 ns / 1 us / 1 ms / 1 s [per 100 eV]:

| Species | water | O2 21 % |
|---|---|---|
| e_aq | 3.34 / 1.60 / 0.065 / 0.004 | 3.31 / 0.013 / 0 / 0 |
| OH | 3.54 / 1.94 / 1.52 / 1.50 | 3.51 / 1.95 / 1.45 / 1.40 |
| H | 0.78 / 0.75 / 1.77 / 1.81 | 0.78 / 0.002 / 0 / 0 |
| H2O2 | 0.44 / 0.67 / 0.71 / 0.72 | 0.44 / 0.73 / 0.81 / 1.46 |
| H2 | 0.41 / 0.71 / 0.80 / 0.80 | 0.40 / 0.58 / 0.58 / 0.58 |
| HO2 | 0.07 / 0.04 / 0.001 / 0.003 | 0.08 / 0.58 / 0.012 / 0.006 |

Qualitative reading (by eye; no tabulated reference G(t) is available in the UHDR example folder, and no external numbers are quoted here). The method of following G(t) of a single track out to long times with this mesoscopic scheme is the one of the UHDR example (arXiv 2409.11993, cited as the method reference only):
- Water: e_aq is gone by about 1 ms, and its disappearance is mirrored by the rise of H (e_aq + H3O+(B) in the pH 7 buffer). OH, H2O2 and H2 level off after about 1 ms. HO2 decays to a noise floor.
- O2: e_aq and H are scavenged by O2 within 1 us, consistent with the 1 us comparison. HO2 peaks near 0.1 to 1 us and decays. H2 stays constant after 1 us.
- Open point: the late H2O2 rise in the O2 run (section 2, item 3). In the raw `SpeciesMeso.csv` O2^- drops from about 9200 to about 4150 counts (sum over 40 events) from 0.13 s while H2O2 grows by about 2500, roughly 2 O2^- per H2O2. Its onset is sharp.

Wall times. Water: 469.7 s total, per-event chemistry wall 99.9 s mean, 158 s maximum (particle stage about 3 s, the rest mesoscopic). O2: 2135.1 s total, per-event 418.6 s mean, 940.9 s maximum (particle stage about 3 to 6 s). The water run overlapped with the 1 core Serial run of section 4, and the start of the O2 run did too, so per-event times are not a clean benchmark; an earlier 2-event 1 s water test on a quiet machine measured about 11 s per event.

## 7. Known limits

- No reaction counts after the hand-over: Geant4 does not report which reaction fired in the mesoscopic stage, so `Reactions_*` cover the particle stage only (ADR 0006).
- The O2 pool is per event: it is consumed in both stages and restored for each event. Multi-track depletion needs several tracks in one event (stage 2).
- `SpeciesMeso.csv` holds summed counts, not per-event sums of squares: no per-event variance, so statistical errors on the post-hand-over part come from batch means or separate runs, not from the file.
- `WrongResolution` warning (`G4DNAEventScheduler::InitializeInMesh`, "resolution is not good : 15.258789") is printed once per event. It is benign: the initial cell is capped at 65536 cells per side by Geant4's 32-bit mesh index, so the cell is 15.26 nm instead of the UHDR 6.25 nm.
- The cell-size effect is not separated from the model difference (section 2).
- The late H2O2 rise in the O2 run is unexplained (section 2, item 3).
- N is small for the long runs (40 events); no statistical error is given for the long-time values.
- Stage-2 items (dose-sized multi-track events, pulse structure, per-event pool carried over) are out of scope here.
