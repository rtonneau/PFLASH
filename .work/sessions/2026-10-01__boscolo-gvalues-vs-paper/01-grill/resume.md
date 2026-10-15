# Session: boscolo-gvalues-vs-paper

**Date:** 2026-10-01T15:32:39.570Z
**Status:** Grill phase complete

## Problem Statement

Under `/chem/select BoscoloChem` (100 keV electrons), time-dependent G-values diverge from Boscolo et al. 2020 (Int. J. Mol. Sci. 21, 424), Figure 3 (500 keV electrons, pO2 0-21%):

- H• at low pO2 decreases until ~1 ns then rises again (0.74 at 1 ns -> 0.88 at 100 ns at 0%); the paper decreases monotonically (~0.82 -> ~0.55 at 0%, -> 0 at 21%).
- HO2° (radical) keeps rising at 1 µs (0.88 at 21%); the paper saturates at 0.66 by ~0.1-0.3 µs. HO2° is already 0.06 at 0% (paper ~0).
- O2⁻ at 21% is 3.14 and still rising at 1 µs; the paper saturates at 2.24.
- (The initial "HO2⁻ too low" report was a species mix-up: the paper's 0.66 is HO2•; HO2⁻ is negligible in the paper and in ours.)

Diagnosis (verified in Geant4 11.4.1 source and reaction counts, scratch runs at 0% and 21%):

1. SBS reaction acceptance ignores k outside the reaction radius. `G4DNAMolecularReactionData::ComputeEffectiveRadius` gives R = k/(4π D N_A) for every reaction; `G4DNASmoluchowskiReactionModel::FindReaction` (line 119) accepts with the Brownian-bridge probability exp(-(r1-R)(r2-R)/(D Δt)), which does not vanish as R -> 0. Slow Table 1 reactions (OH+H2->H k=4.5e7, OH+H2O2->HO2 k=2.3e7, H+OH⁻->e_aq k=2e7, H+H2O2->OH k=1e8) fire about as often as diffusion-controlled ones at late times (e.g. 30-100 ns at 0%: OH+H2 66 vs OH+H 77).
2. No upper bound on the chemistry time step. `TimeStepAction` only sets minimum steps; `G4Scheduler::fMaxTimeStep` = DBL_MAX. Once e_aq/H are scavenged at 21%, steps grow huge and the last bin (300 ns-1 µs) shows a burst (H3O⁺+O2⁻->HO2 235 vs 6 in the previous bin; OH+OH 79 vs 8; OH+H2O2 33 vs 0).
3. O2⁻ excess is mainly a separate, shared-physics effect: e_aq survives the spur longer than in TRAX (G(e_aq,10 ns) 3.24 vs ~2.7), so more e_aq reaches O2.

## Context & Constraints

- Project scope (CLAUDE.md): SBS is the only chemistry time-step model; one homogeneous water box; UHDR is a use case.
- ADR 0002: a Chemistry varies reaction content only, not the time-step model -> the fix must not be tied to BoscoloChem.
- Geant4 allows a custom reaction model: `G4DNAMolecularStepByStepModel(const G4String&, std::unique_ptr<G4VDNAReactionModel>)`; `DnaChemistryList::ConstructTimeStepModel` currently registers `new G4DNAMolecularStepByStepModel()`.
- `G4Scheduler::SetMaxTimeStep(G4double)` exists (default DBL_MAX).
- BoscoloChem's Table 1 transcription was re-checked against the rendered table: correct.
- Comparison at 100 keV electrons (user choice), vs Fig. 3's 500 keV.
- Unit tests must be kernel-free (plain functions), built and run from `build-ninja/` (Debug).
- New logic classes follow the portable-class convention (no dependency on other project classes, include prefix adjusted on copy).
- Runs are slow: a 2-event 100 keV run at 21% took ~35 min before any step cap.

## Success Metrics

With both new switches enabled and BoscoloChem, 100 keV electrons, pO2 0, 0.5, 5, 10, 21%:

- Shape: H• non-increasing after 1 ps at 0%; HO2° and O2⁻ saturate (flat within statistics) before 1 µs at 21%; HO2° ≈ 0 at 0%.
- 1 µs values within ±10% of the paper: O2⁻(21%) 2.24, HO2°(21%) 0.66, e_aq(0%) 2.25, H•(0%) 0.55.
- Unit test: the rate-aware acceptance probability reduces to today's bridge as k -> k_diff and to 0 as k -> 0.
- k-recovery check: in an anoxic run, slow-reaction firing counts (OH+H2, OH+H2O2, H+OH⁻) drop to the rate-equation expectation order of magnitude, before/after.
- Default behavior (switches off) is unchanged: PureWater output statistically identical to today's.
- `analysis/compare_boscolo_fig3.py` produces the 2x2 overlay PNG and a pass/fail table from the dumps.

## Architecture & Approach

- Rate-aware reaction acceptance: a project reaction model (G4VDNAReactionModel, based on G4DNASmoluchowskiReactionModel) passed to G4DNAMolecularStepByStepModel. In-radius test unchanged; the in-between (Brownian-bridge) acceptance made consistent with k for partially diffusion-controlled reactions (contact radius + per-encounter reaction probability). The exact formula is sourced from the literature during planning and isolated in a kernel-free function for unit testing.
- Step cap: `G4Scheduler::SetMaxTimeStep` from a macro value.
- Opt-in, two independent PreInit commands, defaults = today's behavior: `/chem/sbs/rateAwareReactions true|false` (default false) and `/chem/sbs/maxTimeStep <value> <unit>` (default none). Both recorded in `Manifest.json`. `beam_boscolo.in` sets both.
- ADR records the opt-in SBS correction (hard to reverse, surprising, real trade-off).
- Comparison tool: `analysis/compare_boscolo_fig3.py <dump-dir>...` reads `Species_nt_species.csv` + `Manifest.json` (pO2), overlays on `analysis/reference/boscolo2020_fig3.csv` (≈10 hand-digitized points per curve: e_aq, H•, O2⁻, HO2• at 0, 0.5, 5, 10, 21%, citing Fig. 3), writes a 2x2 PNG and a pass/fail table of the shape and ±10% checks.
- `macro/test_run.ps1` gains parameters to emit the two new commands, so the scan can feed the tool.
- e_aq / O2⁻: measure after the SBS fix; if still outside ±10%, a later ticket compares `/process/dna/e-SolvationSubType` options by macro only (no shared-physics code change) and picks the best for `beam_boscolo.in`.

## Assumptions & Trade-offs

- Opt-in rather than global: keeps PureWater baselines unchanged, at the cost of two code paths.
- Step cap trades runtime for correctness; the cap value for `beam_boscolo.in` is chosen by measurement.
- 100 keV vs 500 keV: both low LET; residual differences are accepted within the ±10% tolerance.
- ±10% on e_aq/O2⁻ may not be reachable without the solvation-subtype step (G4 vs TRAX initial yields: G(e_aq,1 ps) 4.04 vs 4.5).
- Fig. 3 has no 0.3% curve: 0.5% is used instead (user choice).

## Open Questions

- Exact rate-aware acceptance formula (to be sourced from the literature in the plan).
- Step-cap value for `beam_boscolo.in` (by measurement).

## Notes

- Scratch runs of today's behavior: `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/runs/{o2_0,o2_21}` (100 keV, 2 events, log-spaced reaction bins 1 ps-1 µs).
- Fig. 3 rendered: `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/fig3.png` (PyMuPDF in `.scratch/tests/2026-09-30__boscolochem-table1/pylib`).
- CONTEXT.md gained "Reaction acceptance" (incl. rate-aware acceptance) and "Step cap".

## Token Usage

- **Input:** 142
- **Output:** 40868
- **Cache read:** 8158791
- **Cache creation:** 104691
- **Total:** 8304492
