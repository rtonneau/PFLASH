# Simulations

All runs use `examples/dose-rate-study` (`history`, `doseCutoff` mode) built against the
**patched** Geant4 11.4.3 install (patches in `patches/geant4/11.4.3/`, see
[geant4-issues.md](geant4-issues.md), issues 1–3). Each history is one single-threaded process.
They ran in parallel on one 20-logical-core workstation, so the wall times include contention.

Data root: `${STUDIES_ROOT}/PFLASH/` (outside git; Studies `2026-11-15__dose-rate-study` and
`2026-11-16__pulse-by-pulse` in [docs/studies/INDEX.md](../studies/INDEX.md)). Paths below are
relative to it. Until 2026-11-18 the data were under `.scratch/tests/`.

## Common setup

| Item | Value |
|---|---|
| Primary | 500 keV e⁻, starts 1 nm inside the −z face of a 20 µm water world, uniform (x, y) over the face of the 4 µm box, along +z |
| Packet library | 2400 packets from 12 parallel captures (`2026-11-15__dose-rate-study/spike/library`). One packet = the molecules inside the box at 1 ps + the energy deposited in it. Mean 653 eV per packet (600 eV per launched primary, hit fraction 0.918). 3.93 Gy in total. |
| Box | 4 µm side (half side 2 µm), 6.4e-14 kg, reflecting walls (ADR 0011). 1 Gy = 3.99e5 eV ≈ 600 tracks. History world half side 3 µm. |
| Physics | `G4EmDNAPhysics_option2` (capture only) |
| Chemistry | PFLASH `PureWater` network (`PureWaterChemistryList`, same `PureWaterReactions.cc` as `src/chemistry/catalog/`): 40 molecules, 12 dissociation channels, 65 pair reactions, 23 bulk reactions. pH 7, fixed in code (`kDefaultPH`): bulk H₃O⁺(B) and OH⁻(B) at 10⁻⁷ M in the 500 µm half-side reservoir (`PureWaterChemistryList.cc:89-90`). No O₂, so the three O₂ bulk reactions are inert. ·OH, H₂O₂ and H₂ have no bulk reaction. IRT_syn only, no mesoscopic stage. Ledger string: "PureWater (copied network + acid-base buffer), pH 7, IRT_syn". |
| Arrivals | Homogeneous Poisson process at the launch rate Ḋ·m / ⟨E per launched primary⟩, thinned by the hit fraction. Each arrival injects the next library packet, activated 1 ps after its arrival time. Each run starts at its own random library offset (`firstPacket`). |
| Stop | First arrival whose cumulative deposit reaches the target dose; beam-off = 1 ps after the last activation, chemistry stops there. No post-beam relaxation. |
| Contacts | Each new track's molecules are trialled against the molecules already present (timed-injection patch) |

## 2026-11-15: dose-rate production study (earlier session, input to this one)

Aim: G(t) and G at beam-off vs dose rate, up to 1 Gy. Full record: `docs/dose-rate/dose-rate-study.md`.

| Set | Rates (Gy/s) | Realisations | Dose | Notes |
|---|---|---|---|---|
| `prod/plan_1Gy` + `prod/plan_lo1Gy` | 50, 100, 500, 10³, 10⁴ + 0.1 | 10 each; 0.1 Gy/s: 6 used of 7 | 1 Gy | `r0.1_003` crashed at 8.79 s (issue 8). Steady-state stop on (50-sample windows), never fired. Grid: absolute log, 10 per decade. Seeds: chemistry 1000+, source 500000+. Cap 3 h. |
| `prod/plan_lo` | 0.1 | — | 0.5 Gy | side set |
| `prod/snap_*` | one per rate | 1 | 1 Gy / 0.5 Gy | `snapshots = true`, for the movies (`scripts/movie.py`). The 0.1 Gy/s, 1 Gy snapshot run crashed at 9.78 s (issue 8). |

Cost: 2.0e5 s summed for 50 to 10⁴ Gy/s; 1.9e5 s for the 0.1 Gy/s runs (3.1e4 s each, set by the
9.6 s of simulated time). Peak memory 238–300 MB.

## 2026-11-16: pulse-by-pulse runs (`2026-11-16__pulse-by-pulse/`)

**Aim:** resolve every track individually. The same `history`, plus a per-track age grid
(`ageGridPointsPerDecade = 20`): after each arrival the box is sampled on a log grid of the
**age** since that arrival, up to the next arrival (`species_age.csv`). This gives per-track G
(see [methods.md](methods.md)) and the pulse-vs-pulse comparison (early vs late tracks in the same run).

Study configs: `study_pulse.json`, `study_pulse_lo.json`; plans written by
`examples/dose-rate-study/studies/make_configs.py`, run by `studies/run_study.ps1`. Steady-state
stop off (`steadyStateWindow = 0`), no wall-time cap.

| Plan | Rates (Gy/s) | Realisations | Dose | Seeds (chemistry / source) |
|---|---|---|---|---|
| `plan_pulse` | 50, 10³, 10⁴ | 3 each | 1 Gy | 71000+ / 710000+ |
| `plan_pulse_lo` | 0.1 | 3 | **0.1 Gy** | 72000+ / 720000+ |

0.1 Gy/s is run to 0.1 Gy only, because 1 Gy takes about 10 s of simulated time, 3e4 s of
wall time, and lies above the counter-crash limit (issue 8). Its tracks arrive about 17 ms
apart on average, so each track is followed for that long (its gap to the next arrival) before the next one lands.

## 2026-11-16/17: the same runs with the reaction log (`plan_pulse_rx`, `plan_pulse_lo_rx`)

**Aim:** count every reaction with the track of origin of each reactant, to measure *who
destroys whose* molecules (own track, other tracks, bulk buffer) instead of only inferring it
from net counts. Same configs and seeds as `plan_pulse` / `plan_pulse_lo`, rebuilt `history` with
`ReactionLog` (`reactions.csv` + ledger `reactions` block). Code: [reproduction.md](reproduction.md).

Per-run results (`runner_pulse_rx.out`, `runner_pulse_lo_rx.out`, `*/run_log.csv`):

| Rate (Gy/s) | Tracks per run | Beam-off | Wall time, pulse / rx (s) | Reactions per run | interSource | positionMatched | Peak memory (MB) |
|---|---|---|---|---|---|---|---|
| 0.1 (to 0.1 Gy) | 50, 65, 70 | 0.61–1.17 s | 201–432 / 215–466 | 5.8–7.1k | 896–1160 | 1332–1921 | 53–130 |
| 50 | 597–605 | 19.5–20.1 ms | 2740–2810 / 3055–3112 | 58.3–58.9k | 12.8–13.0k | 6789–6923 | 670–675 |
| 10³ | 568–572 | 0.90–0.97 ms | 4484–4619 / 4880–5093 | 48.9–49.4k | 11.4–11.6k | 2559–2693 | 612–613 |
| 10⁴ | 560–636 | 94–103 µs | 5917–6382 / 6339–6848 | 31.9–32.4k | 5.7–6.5k | 654–735 | 518–545 |

- `unknownOrigin = 0` in all 12 runs: every reactant was attributed to a track or to the bulk.
- `interSource`: pair reactions whose two reactants come from different tracks.
- `positionMatched`: products of bulk (scavenger) reactions attributed by position, because
  Geant4 gives them no parent (issue 9).
- The `bulk` counts printed by these runs (3.4k–30.6k) are **inflated**: they include
  pre-chemical water (`H2O^0`, `H2O^+`) logged as vanished. The water skip in
  `ActivationObserver` was added after these runs (rebuilt, tests pass). `rx_analysis.py` filters
  those rows, so the analysis is not affected. A rerun would give clean ledger counts.
- The reaction log costs about 10 % of wall time.
- The rx runs have the same arrivals and end times as their `plan_pulse` twins, but not the
  same chemistry: step counts differ slightly (issue 15). Compare them statistically.
- Two aborted launches of `plan_pulse_lo_rx` (`run_log_aborted1.csv`, `run_log_aborted2.csv`)
  were stopped by hand while the water skip was being fixed. They are not data.

## `plan_quick` (determinism check)

`plan_quick/out/q` and `plan_quick/det/r0.1_002`: two short repeats of one config with the same
binary. Same result, so a given build is deterministic for given seeds. `plan_quick/det.log` holds
the `ProcMan113` warnings of issue 14.

## Earlier sessions (context only)

| Directory | Topic | Record |
|---|---|---|
| `2026-11-06__timed-track-injection-dose-rate-feasibility` | Delayed injection of tracks into one IRT_syn run | `docs/dose-rate/recurring-chemistry-sources.md` |
| `2026-11-08__molecular-finder-degenerate-coordinates` | Octree coincident duplication (issue 1) | `docs/dose-rate/molecular-finder.md` |
| `2026-11-08__beam-on-irt-beam-off-meso`, `__restricted-spatial-coupling*` | Mesoscopic beam-off and restricted coupling | `docs/dose-rate/mesoscopic-feasibility.md`, `restricted-coupling-feasibility.md` |
| `2026-11-09__*` | Inter-track validation, reusable `poisson-track-irt` example | `examples/poisson-track-irt/docs/` |
| `2026-11-10__poisson-track-irt-stock-build`, `2026-11-14__revert-ptirt-stock-build` | Stock-build attempt and its revert | ADR 0010 |
| `2026-11-14__dose-rate-gvalue-study` | Study tooling, timing spike | `docs/dose-rate/README.md` |
