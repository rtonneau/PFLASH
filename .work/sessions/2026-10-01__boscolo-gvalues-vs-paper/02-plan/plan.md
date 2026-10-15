# Implementation Plan

**Session:** boscolo-gvalues-vs-paper
**Date:** 2026-10-01T16:47:34.033Z
**Estimated effort:** 1.5 days (about half of it unattended simulation time)

## Strategy

Fix the two SBS artifacts behind the BoscoloChem / Fig. 3 mismatch, both opt-in, then measure against digitized Fig. 3 curves.

1. Exact 3D encounter probability. Geant4's in-between test (`G4DNASmoluchowskiReactionModel.cc:119`) uses P_G4 = exp(-(r1-R)(r2-R)/(D·Δt)). That is the large-separation limit of the hitting probability of the radial (Bessel-3) process conditioned on its endpoints. The exact form, from the method of images on u = r·p with an absorbing sphere at R, is

   P = [exp(-(r1-R)(r2-R)/(D·Δt)) - exp(-r1·r2/(D·Δt))] / [1 - exp(-r1·r2/(D·Δt))]

   P -> 0 as R -> 0 and P -> P_G4 when R·(r1+r2) >> D·Δt (their ratio is 1 - exp(-(R(r1+r2) - R²)/(D·Δt)) up to the denominator). With Geant4's R = k/(4π·D·N_A), the long-time rate is the Smoluchowski rate k, so slow reactions stop firing at the encounter rate. No contact radius is needed, and TRAX uses the same R. This resolves the grill's open "formula" question.
2. Step cap: `G4Scheduler::SetMaxTimeStep` (default `DBL_MAX`, read at the start of every `G4Scheduler::Stepping()`).
3. Both are off by default: `/chem/sbs/rateAwareReactions true|false` and `/chem/sbs/maxTimeStep <value> <unit>`, PreInit, stored on `DnaChemistryList` like the reaction time-binning settings, and recorded in `Manifest.json`.
4. A comparison tool (`analysis/`) against hand-digitized Fig. 3 points, then a validation scan at 100 keV, pO2 0, 0.5, 5, 10, 21%.

Global constraints (every ticket):
- C++20, Geant4 conventions, PascalCase files and classes, camelCase variables; match the surrounding file style.
- Includes are rooted at `header/` (`#include "chemistry/RateAwareReactionModel.hh"`).
- Kernel-free logic gets a plain-`assert` CTest in `test/`, built and run from `build-ninja/` (Debug), with the MSVC CRT preamble (`.claude/geant4-instructions.md` section 5). `sim` is built in `build/` (RelWithDebInfo). Build with the MSVC env (section 1) through the PowerShell tool.
- Logging goes through `DnaLogger`; no ad hoc `G4cout`.
- Defaults (both switches unset) must build exactly today's objects: `new G4DNAMolecularStepByStepModel()` and no `SetMaxTimeStep` call.
- Scratch runs and artifacts go under `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/`. Scratch macros go into `build/macro/`, not the tracked `macro/`.
- Test runs use 100 keV electrons, `/run/beamOn 2`, BoscoloChem. A 2-event 21% run takes about 35 min: run sims in the background and run at most 3 in parallel.

## Tickets Overview

1. `01-encounter-probability`: kernel-free `EncounterProbability` function + unit test.
2. `02-rate-aware-reaction-model`: Geant4 reaction model using it, `/chem/sbs/rateAwareReactions`, manifest key, ADR 0006, docs.
3. `03-sbs-max-time-step`: `/chem/sbs/maxTimeStep`, manifest key, docs.
4. `04-fig3-comparison-tool`: digitized reference CSV + `analysis/compare_boscolo_fig3.py`; must FAIL on today's scratch data.
5. `05-scan-script-and-k-recovery`: `test_run.ps1` switches; anoxic before/after check that slow reactions stop over-firing.
6. `06-boscolo-validation-scan`: 5-level scan with both switches, comparison table, `beam_boscolo.in` update.
7. `07-solvation-subtype-comparison`: conditional; only if e_aq or O2⁻ miss ±10% in ticket 6.

## Sequencing Rationale

Ticket 1 is pure math with the strongest test, so it goes first. Ticket 2 depends on it. Ticket 3 is independent of 2, but it touches the same messenger, manifest and doc spots, so it goes right after. Ticket 4 (the tool) is checked on existing scratch data, so it doesn't wait for any sim. Ticket 5 needs 2+3 to be built and uses the scan script that ticket 6 reuses. Ticket 6 needs everything. Ticket 7 only runs if ticket 6's e_aq/O2⁻ checks fail.

## Risks & Mitigation

- The exact formula may still leave a residual end-of-chemistry burst. Mitigation: the step cap (ticket 3), checked in ticket 6 by bin-to-bin reaction counts.
- e_aq and O2⁻ may stay outside ±10% (G4 vs TRAX initial yields and spur dynamics). Mitigation: ticket 7 compares solvation subtypes by macro; whatever the outcome, report it honestly.
- Step-cap runtime: 1 ns equals the existing minimum step beyond 10 ns (`TimeStepAction`), so the extra steps are bounded (~1000 after 10 ns). Ticket 6 records the wall times from `Manifest.json`.
- Digitization error of about ±0.02 G is well inside ±10% of the checked plateau values.
- MT: settings live on `DnaChemistryList` (shared object, set in PreInit before workers start), and `SetMaxTimeStep` is applied per thread where `ApplyReactionTimeBinning` already is (`RunAction::BeginOfRunAction`).

## Assumptions

- `G4DNASmoluchowskiReactionModel::FindReaction` is virtual (it is: it overrides `G4VDNAReactionModel`), and the `G4DNAMolecularStepByStepModel(name, std::unique_ptr<G4VDNAReactionModel>)` constructor installs the given model.
- `Species_nt_species.csv`: G at time t = `sumG / nEvent` (checked on the scratch runs: G(°OH, 1 ps) = 4.84).
- pO2 in % = scavenger O2 molarity / (0.0013 M) × 100 (`/chem/env/scavenger ... %` uses kH = 0.0013 M).
- Fig. 3 is 500 keV; our comparison is 100 keV (user decision).

## Token Usage

- **Input:** 16
- **Output:** 19815
- **Cache read:** 1461106
- **Cache creation:** 29599
- **Total:** 1510536
