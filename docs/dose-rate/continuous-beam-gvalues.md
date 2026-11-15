# Dose-rate dependence of G-values at 1 ms, continuous 100 keV e⁻ beam (pure water)

## Question

Does the dose rate change the radiolysis G-values of pure water at 1 ms, for rates from conventional (0.1 Gy/s) up to 1000 Gy/s, under a continuous beam?

## Setup

- **Tool:** `examples/poisson-track-irt` (`capture` → `history`), patched Geant4 11.4 build in `build-patched`.
- **Beam:** 100 keV electrons. Each primary starts on the −z entry face of a 5 µm water box (half side 2.5 µm, mass m = 1.25e-13 kg).
- **Physics and chemistry:** the example's DNA physics; the example's pure-water reaction set without O₂; IRT_syn to 1 ms. The walls reflect the molecules.
- **Packet library:** 37000 single-primary packets.
  - Mean deposit: 1826 eV per packet.
  - 15 events left no precursor molecule (0 eV deposited). They were skipped with `--max-empty`.
- **Arrivals:** a homogeneous Poisson process with λ = Ḋ·m / ⟨E_dep⟩. Each arrival injects a randomly drawn packet at its arrival time. Random draws with no arrival in 1 ms contribute zero dose and zero molecules: they are counted (`n_empty`) but not simulated.

| Rate (Gy/s) | Realizations | Simulated (non-empty) | Mean tracks / realization | Mean dose at 1 ms (Gy) |
|---|---|---|---|---|
| 0.1 | 160000 | 6726 | 0.043 | 1.0e-4 |
| 1 | 16000 | 5613 | 0.43 | 1.0e-3 |
| 10 | 1600 | 1579 | 4.3 | 0.010 |
| 100 | 160 | 160 | 43 | 0.10 |
| 1000 | 16 | 16 | 428 | 1.02 |

Realizations were chosen so that every rate gets about the same number of tracks, about 6.5k.

## How G and its SE are computed

G(t) = 100 · ΣN_species(t) / ΣE_dep(t) [eV], summed over all realizations of a rate, empty ones included. This is a ratio estimator. Its standard error comes from the delta method over realizations.

The change from the reference is ΔG = G(Ḋ) − G(0.1 Gy/s). Its SE is the two SEs added in quadrature, since the samples are independent. In the tables, "significant" means |ΔG| > 3 SE.

## Results: G at 1 ms (molecules / 100 eV, ± SE)

| Species | 0.1 Gy/s | 1 Gy/s | 10 Gy/s | 100 Gy/s | 1000 Gy/s |
|---|---|---|---|---|---|
| e_aq⁻ | 2.325 ± 0.012 | 2.338 ± 0.012 | 2.259 ± 0.012 | 1.663 ± 0.015 | 0.375 ± 0.007 |
| ·OH | 2.380 ± 0.013 | 2.383 ± 0.012 | 2.333 ± 0.012 | 1.940 ± 0.013 | 0.889 ± 0.010 |
| H | 0.688 ± 0.002 | 0.680 ± 0.002 | 0.686 ± 0.002 | 0.740 ± 0.002 | 0.438 ± 0.005 |
| H₂ | 0.492 ± 0.003 | 0.488 ± 0.003 | 0.499 ± 0.003 | 0.610 ± 0.003 | 0.928 ± 0.003 |
| H₂O₂ | 0.701 ± 0.003 | 0.700 ± 0.003 | 0.698 ± 0.003 | 0.731 ± 0.003 | 0.794 ± 0.002 |
| H₃O⁺ | 3.037 ± 0.011 | 3.049 ± 0.011 | 2.993 ± 0.011 | 2.453 ± 0.016 | 0.725 ± 0.011 |
| OH⁻ | 0.678 ± 0.002 | 0.678 ± 0.002 | 0.699 ± 0.002 | 0.749 ± 0.003 | 0.314 ± 0.004 |

ΔG vs 0.1 Gy/s (± SE):

| Species | 1 Gy/s | 10 Gy/s | 100 Gy/s | 1000 Gy/s |
|---|---|---|---|---|
| e_aq⁻ | +0.013 ± 0.017 | −0.066 ± 0.017 | −0.662 ± 0.019 | −1.950 ± 0.014 |
| ·OH | +0.004 ± 0.017 | −0.046 ± 0.017 | −0.439 ± 0.017 | −1.490 ± 0.015 |
| H | −0.007 ± 0.003 | −0.002 ± 0.003 | +0.052 ± 0.003 | −0.250 ± 0.005 |
| H₂ | −0.003 ± 0.004 | +0.007 ± 0.004 | +0.119 ± 0.004 | +0.436 ± 0.003 |
| H₂O₂ | −0.000 ± 0.004 | −0.002 ± 0.004 | +0.030 ± 0.003 | +0.093 ± 0.003 |
| H₃O⁺ | +0.012 ± 0.015 | −0.043 ± 0.016 | −0.583 ± 0.019 | −2.311 ± 0.015 |
| OH⁻ | +0.000 ± 0.003 | +0.021 ± 0.003 | +0.071 ± 0.003 | −0.363 ± 0.004 |

### What changes, and from which rate

- **1 Gy/s:** no resolvable effect for any species at this precision. H changes by −0.007 ± 0.003 (2.3 SE), which is below the threshold.
- **10 Gy/s:** the onset.
  - e_aq⁻ −0.066 ± 0.017 (3.9 SE) and OH⁻ +0.021 ± 0.003 are significant.
  - ·OH (−2.7 SE) and H₃O⁺ (−2.7 SE) are borderline.
  - The others are unresolved.
- **100 and 1000 Gy/s:** a strong effect for every species.
  - The radicals and H₃O⁺ fall: at 1000 Gy/s, G(e_aq⁻) drops by 84 % and G(·OH) by 63 %.
  - The molecular products rise: G(H₂) nearly doubles, and G(H₂O₂) gains +13 %.
  - H and OH⁻ rise at 100 Gy/s, then fall at 1000 Gy/s.

The G(t) curves show the mechanism. All rates agree up to about 10⁵ ns. After that, at 100 and 1000 Gy/s, e_aq⁻ and ·OH are consumed by reactions between tracks. At 1000 Gy/s, about 400 tracks share 125 µm³ within 1 ms. Early-time points (< 1 µs) are noisy: few tracks have arrived by then at the low rates.

## Figures

The figures are slide-ready, made with `pptfig`. Each figure directory also holds an SVG, a separate `__legend.png` and a manifest.

- `continuous-beam-gvalues/g1ms_vs_rate/g1ms_vs_rate__final.png`: G(1 ms) vs dose rate, all species.
- `continuous-beam-gvalues/delta_vs_ref/delta_vs_ref__final.png`: ΔG from the 0.1 Gy/s reference.
- `continuous-beam-gvalues/gt_eaq/`, `gt_oh/`, `gt_h2/` and `gt_h2o2/`: G(t) per dose rate, with ±1 SE bands.

## Limits

- **The box:** it is 5 µm, not 0.5 mm, and its walls reflect molecules. Diffusion out of the irradiated volume is suppressed, so the dose rate at which tracks start to interact is specific to this geometry. It should not be read as a property of bulk water.
- **Dose:** there is no charged-particle equilibrium. Most of each 100 keV primary escapes, and dose is the energy deposited in the box.
- **Chemistry:** pure water (the example's reaction set); no dissolved O₂ and no pH buffer.
- **End time and arrivals:** the end time is 1 ms. The beam is continuous with Poisson arrivals; there is no pulse structure.
- **Total dose:** it scales with the rate (1e-4 to 1 Gy at 1 ms). The study cannot separate the effect of dose rate from that of total dose at a fixed time.
- **Empty events:** the 15 empty capture events are excluded from the packet library. They deposited 0 eV, so the dose bookkeeping is unaffected.

## Evidence and reproduction

Data:
- `continuous-beam-gvalues/gvalues_1ms.csv`
- `continuous-beam-gvalues/gvalues_time.csv`

Raw data: `.scratch/tests/2026-11-14__dose-rate-gvalue-study/production/` (local only). It contains:
- the library (checked with `capture-check`: `CAPTURE_CHECK_OK packets=37000`);
- the plan and the run log (14094/14094 runs ok);
- the per-run outputs.

Reproduction (see `examples/poisson-track-irt/studies/continuous-beam/README.md`):

```powershell
# library (~20 min)
capture --macro capture-100keV-entry-face.mac --count 37000 --first-seed 200001 --max-empty 2000 --out production/library
# plan, runs, analysis (from studies/continuous-beam; ~8 h on 8 slots, dominated by the 16 runs at 1000 Gy/s)
python make_schedules.py --library production/library --study study.json --out production/plan
pwsh run_study.ps1 -Plan production/plan/plan.json -Parallel 8 -LogCsv production/plan/run_log.csv
python analyze.py --plan production/plan/plan.json --out docs/dose-rate/continuous-beam-gvalues
```

Figures: `continuous-beam-gvalues/make_figures.py`, run from the repo root with `PPT_RESSOURCES=docs/dose-rate/continuous-beam-gvalues`.
