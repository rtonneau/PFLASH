# SBS regression against the 1d047c4 reference

Does the SBS mode of the current build ([[0008-selectable-chemistry-modes]]) reproduce the SBS reference in `validation/reference/sbs_{water,o2}`? 10 keV e-, `PureWater` Chemistry, seeds `12345 67890`, Ritchie1994 solvation, end time 1 us, MT x10, G(1 us) per 100 eV at t = 999.999 ns.

**Result.** Before the fix: FAIL. SBS inherited the partially diffusion-controlled reaction type (type 1) that `f50eb2c` added for IRT_syn, and e_aq, OH, H2O2 and H2 came out 12 to 30 % low. After the fix (SBS keeps every reaction type 0): water N = 40 passes both gates for all five species; O2 N = 300 passes both gates for e_aq, H2O2 and H2, and OH and H pass only the combined gate (see section 4). IRT_syn is unchanged.

Caveat: the references were captured with an older build (`1d047c4`, SBS only, all reactions type 0). The comparison is between two builds, not a rerun of the same code.

## 1. Commands and gate

Macros: `macro/validate_water.in` and `macro/validate_o2.in` with one line added before `/run/initialize` (`/process/chem/TimeStepModel SBS`) and, for O2, `/run/beamOn 300` instead of 1500. Seeds, 1 us and 10 keV are unchanged; the macros in `macro/` are untouched. Run from `build/` (RelWithDebInfo):

```bash
./sim.exe <scratch>/07-validate_water.in --threads 10 --dir <scratch>/<out>   # N = 40
./sim.exe <scratch>/07-validate_o2.in    --threads 10 --dir <scratch>/<out>   # N = 300
python analysis/compare_reference.py validation/reference/sbs_water <scratch>/<out>
python analysis/compare_reference.py validation/reference/sbs_o2    <scratch>/<out>
```

`<scratch>` is `.scratch/tests/2026-10-08__verify-feature-combinations-and-add-three-example-macros-with` (files prefixed `07-`). Python: conda env `GEANT4` (`GEANT4_py311` named in the script does not exist on this machine). All runs: exit 0, no `EEEE`/`WWWW`. `compare_reference.py` rejected SBS dumps because the manifest carries `chemistryModel` in every mode; it now tests `mesoEnabled` (falling back to `chemistryModel` for old dumps).

Gate, per species: |G_new - G_ref| <= 3 x SE_ref (reference error, README of each reference) and <= 3 x sqrt(SE_ref^2 + SE_new^2). SE is recomputed from `sumG`/`sumG2` as in the README (mean = sumG/N, variance = (sumG2 - sumG^2/N)/(N-1), SE = sqrt(variance/N)); H is not in the README tables, its SE comes from the CSV the same way. "/SE_ref" and "/SE_comb" are the difference in those units.

## 2. Before the fix: SBS with type 1 inherited (FAIL)

Build `125160a` (HEAD before the fix). Wall times 110.8 s (water; reference 109.4 s) and 974.2 s (O2, N = 300; reference 5227.3 s for N = 1500).

Water, N = 40 (reference 40):

| Species | G_ref | SE_ref | G_new | SE_new | diff | /SE_ref | /SE_comb | gate |
|---|---|---|---|---|---|---|---|---|
| e_aq | 1.2445 | 0.0319 | 0.9270 | 0.0246 | -0.3175 | -9.96 | -7.89 | FAIL |
| OH | 1.5973 | 0.0354 | 1.2768 | 0.0346 | -0.3205 | -9.06 | -6.47 | FAIL |
| H2O2 | 0.4293 | 0.0106 | 0.3478 | 0.0091 | -0.0815 | -7.73 | -5.86 | FAIL |
| H2 | 0.7130 | 0.0112 | 0.5875 | 0.0097 | -0.1255 | -11.20 | -8.47 | FAIL |
| H | 0.8752 | 0.0141 | 1.1155 | 0.0183 | +0.2402 | +17.09 | +10.43 | FAIL |

O2 21 %, N = 300 (reference 1500; the new error bars are about 2 times wider, taken from the dump):

| Species | G_ref | SE_ref | G_new | SE_new | diff | /SE_ref | /SE_comb | gate |
|---|---|---|---|---|---|---|---|---|
| e_aq | 0.0131 | 0.00030 | 0.0091 | 0.00055 | -0.0040 | -13.41 | -6.40 | FAIL |
| OH | 1.9988 | 0.0062 | 1.6134 | 0.0128 | -0.3855 | -61.78 | -27.15 | FAIL |
| H2O2 | 0.5387 | 0.0017 | 0.4715 | 0.0039 | -0.0672 | -38.73 | -15.87 | FAIL |
| H2 | 0.4518 | 0.0016 | 0.3324 | 0.0031 | -0.1194 | -74.03 | -33.96 | FAIL |
| H | 0.0071 | 0.00022 | 0.0096 | 0.00059 | +0.0025 | +11.25 | +3.93 | FAIL |

## 3. Cause and fix

Checked against the old SBS code:

- Minimum time steps: `TimeStepAction::StartProcessing` adds the same five `AddTimeStep` pairs as `b26a262^` and `1d047c4` (SBS only, once per thread). Identical.
- Bulk-reaction registration: the per-molecule `G4DNAScavengerProcess` registration is unchanged (`MakeBulkReactionData` is a refactor); the bulk reactions are now also added to the reaction table (`f50eb2c`, for the mesoscopic stage).
- Reaction table: same pairs and rates as `1d047c4`, but 37 pairs of `PureWater` (22 of `BoscoloChem`, 52 of `Tonneau2025`) now call `SetReactionType(1)` (`partial()`; every pair `G4EmDNAChemistry_option3` lists as Type II or IV), where the reference had type 0 everywhere. That was the only physics difference found.

A scratch copy of that HEAD with `partial()` at type 0 (`07-src`, `07-build`) reproduced the reference (water all within 1.1 SE_ref; O2 as in section 4), which confirmed the cause before the fix was made.

Fix (decision: the reaction type depends on the time-step model). `SetReactionType(1)` cannot be undone, so SBS must never set it. `ChemistryTypes::PartialReactionsEnabled()` / `SetPartialReactionsEnabled()` (`header/chemistry/ChemistryTypes.hh`, default true, standard library only) is set by `DnaChemistryList::CheckTimeStepModel` at `/run/initialize` (`false` for SBS), before the Chemistry builds its reaction table and bulk list. It is read at every place that sets type 1: `partial()` in `PureWaterReactions.cc` and `BoscoloChemReactions.cc`, the type 1 branch of `Tonneau2025Reactions.cc`, and `MakeBulkReactionData` (the acid-base types 6/7/8 are not diffusion classes and are always applied). The reaction-type counts are logged at `DnaLogger` Info level. Unit test: `TestPartialReactionsSwitch` in `test/ChemistryRegistryTest.cc`; ctest 16/16 passed in `build-ninja/`.

Reaction-type counts logged (`/dnaLogger/verbose Info`, type 0 / type 1 / other):

| Chemistry | IRT_syn | SBS |
|---|---|---|
| PureWater | 22 / 37 / 6 | 59 / 0 / 6 |
| BoscoloChem | 4 / 22 / 0 | 26 / 0 / 0 |
| Tonneau2025 | 25 / 52 / 6 | 77 / 0 / 6 |

## 4. After the fix: SBS with type 0 (real build)

Same commands, same seeds. Wall times 113.1 s (water) and 931.9 s (O2, N = 300).

Water, N = 40:

| Species | G_ref | SE_ref | G_new | SE_new | diff | /SE_ref | /SE_comb | gate |
|---|---|---|---|---|---|---|---|---|
| e_aq | 1.2445 | 0.0319 | 1.2637 | 0.0286 | +0.0192 | +0.60 | +0.45 | PASS |
| OH | 1.5973 | 0.0354 | 1.6358 | 0.0348 | +0.0385 | +1.09 | +0.78 | PASS |
| H2O2 | 0.4293 | 0.0106 | 0.4242 | 0.0100 | -0.0050 | -0.47 | -0.34 | PASS |
| H2 | 0.7130 | 0.0112 | 0.7122 | 0.0105 | -0.0008 | -0.07 | -0.05 | PASS |
| H | 0.8752 | 0.0141 | 0.8812 | 0.0174 | +0.0060 | +0.43 | +0.27 | PASS |

O2 21 %, N = 300 (new SE from the dump, about 2 times the reference):

| Species | G_ref | SE_ref | G_new | SE_new | diff | /SE_ref | /SE_comb | gate vs SE_ref | gate combined |
|---|---|---|---|---|---|---|---|---|---|
| e_aq | 0.0131 | 0.00030 | 0.0128 | 0.00065 | -0.0003 | -0.94 | -0.39 | PASS | PASS |
| OH | 1.9988 | 0.0062 | 2.0343 | 0.0136 | +0.0355 | +5.69 | +2.38 | FAIL | PASS |
| H2O2 | 0.5387 | 0.0017 | 0.5369 | 0.0037 | -0.0019 | -1.08 | -0.46 | PASS | PASS |
| H2 | 0.4518 | 0.0016 | 0.4512 | 0.0038 | -0.0006 | -0.37 | -0.15 | PASS | PASS |
| H | 0.0071 | 0.00022 | 0.0080 | 0.00052 | +0.0008 | +3.84 | +1.50 | FAIL | PASS |

OH (+1.8 %) and H (+12 %) are outside 3 x SE_ref of the reference, which is tight (0.3 % and 3 %) because it has 1500 events. They are inside the combined error of the N = 300 run. Strictly the first gate fails for these two; whether that is a statistical fluctuation or a small real difference would need more events (N = 1500). The other species agree to better than 1.1 SE_ref. `compare_reference.py` (relative tolerance 10 %): both runs PASS (max |rel.diff| 2.41 % water, 2.14 % O2).

Difference to the scratch type-0 build: every row of `Species_nt_species.csv` is identical (98 of 98 rows, both water and O2). The seeded MT runs are reproducible in this setup, and the fix is the same physics as the scratch change.

## 5. IRT_syn is unchanged

IRT_syn Serial runs are not bit-reproducible here (two runs of the same old build, same seed, differ), so a file comparison is not possible. Checked instead:

- The type-1 pair counts under IRT_syn are the pre-fix ones: 37 for `PureWater` (the number of `partial()` calls), 22 `BoscoloChem`, 52 `Tonneau2025` (table in section 3).
- IRT_syn only (`/chem/meso/enable false`, water, N = 40, MT x10, 28.6 s) against the SBS water reference: e_aq +16.6 %, OH +7.3 %, H2O2 +43.0 %, H2 +2.5 %. `docs/irt-syn-mesoscopic-validation.md` section 2 (D1) gives +16.2 %, +7.3 %, +42.7 %, +2.6 % for the same configuration before the fix.

## 6. Conclusion

SBS now reproduces the SBS reference (water: all five species within 1.1 SE_ref; O2 N = 300: e_aq, H2O2, H2 within 1.1 SE_ref, OH and H within the combined error only). The 12 to 30 % drift was the unconditional type 1; SBS registers every reaction as type 0 and IRT_syn keeps type 1. Open: an O2 run with N = 1500 would settle the OH and H first-gate misses; it was not run (about 80 minutes).
