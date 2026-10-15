# Ticket 07: sbs-regression

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja`: 16/16 passed (re-run in this session). Regression on the real build after the fix (MT x10, seeds and 1 us as in the reference, exit 0): water SBS N=40 (113 s) passes both gates for e_aq, OH, H2O2, H2, H (differences +0.60, +1.09, -0.47, -0.07, +0.43 SE_ref). O2 21 % SBS N=300 (932 s): e_aq, H2O2, H2 pass both gates (-0.94, -1.08, -0.37 SE_ref); OH (+1.8 %, +5.69 SE_ref, +2.38 SE_comb) and H (+12 %, +3.84 SE_ref, +1.50 SE_comb) pass only the combined gate (the reference SEs are tiny, N=1500; an N=1500 rerun was not done). compare_reference.py at 10 % tolerance: pass for both (max 2.41 % water, 2.14 % O2). Full numbers: docs/sbs-regression.md.

## Review Notes

First run FAILED the gate (water -18 to -26 %, O2 -12 to -30 %). Cause: since f50eb2c the catalogs set reaction type 1 (partial diffusion control, an IRT_syn setting) for 37 pairs and SBS inherited it; a scratch type-0 build reproduced the reference. The user chose to make the type depend on the model. Fix: `ChemistryTypes::PartialReactionsEnabled()` (atomic, default true), set to false under SBS in `DnaChemistryList::CheckTimeStepModel`, read by the three catalogs and `MakeBulkReactionData` (the type is left unset, because SetReactionType(1) cannot be undone). Reaction types Info line: PureWater IRT_syn 22/37/6 vs SBS 59/0/6 (type 0/1/other). IRT_syn is unchanged as far as checkable: type-1 counts 37/22/52, and an IRT_syn-only water run gives the same differences to SBS as the documented ones (+16.6/+7.3/+43.0/+2.5 % vs +16.2/+7.3/+42.7/+2.6 %); serial IRT_syn runs are not bit-reproducible, so no file comparison. compare_reference.py now reads `mesoEnabled`, because the manifest has `chemistryModel` in every mode. Reference was captured with an older build (1d047c4).

## Blockers / Challenges

The conda env named in compare_reference.py (GEANT4_py311) does not exist here; GEANT4 was used.

## Commits

- 620f77f fix(chem): SBS keeps reactions fully diffusion-controlled, add SBS regression report (ticket 07)

## Time Spent

1h 11m (ticket-start.js to ticket-complete.js)
