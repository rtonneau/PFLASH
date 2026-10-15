# Session: homogeneous-chemistry-prechemical-extension

**Date:** 2026-10-06T17:11:16.161Z
**Status:** Grill phase complete

## Problem Statement

Tonneau et al., Phys. Med. Biol. 70 (2025) 235021 describe a homogeneous pure-water radiolysis network (Table 2, 73 reactions) solved as an ODE from 100 ns, starting from Boscolo G-values; the paper states its formalism cannot reach the earlier phases. dnachem-min has no Chemistry reproducing that network. Add a new selectable Chemistry, `Tonneau2025`, that reproduces Table 2 on tracked Geant4-DNA molecules, so the same network acts from the pre-chemical stage onward (the earlier stages of water radiolysis).

## Context & Constraints

- **Current behavior:** two Chemistries exist (`PureWater`, `BoscoloChem`), selected once per process with `/chem/select`. `PureWater` carries most of the network with Geant4-DNA UHDR rates and reaction types, not Table 2's rates. All Chemistries share the dissociation channels and the molecule set built by `G4ChemDissociationChannels_option1` (which already defines O3 and O3m; HO3 does not exist).
- **Pain point:** the paper's network (its rates, its acid-base block, its O3/HO3 chemistry) cannot be run in the Geant4-DNA track-structure chemistry.
- **Dependencies:** keep the shared dissociation channels, physics, IRT_syn then mesoscopic hand-over, `DnaChemistryWorld` scavenger/pH plumbing and output formats unchanged for the other Chemistries. ADR 0002 (named Chemistries) is relaxed for extra molecules; ADR 0001 and 0004 still hold.
- **Tech stack:** C++20, Geant4 11.4.1 (G4MoleculeTable, G4DNAMolecularReactionTable), CTest plain-assert tests in `build-ninja/`.

## Success Metrics

- `/chem/select Tonneau2025` runs a 10 keV electron smoke run (`/run/beamOn 2`, end time 1 ms) to completion and produces the usual outputs including an HO3 column.
- A unit test confirms the reaction tables, bulk list and HO3 molecule data match Table 2 (reaction count, k, products, types).
- Species yields at about 100 ns from the smoke run are compared with the paper's G-values (Table 3) by a tracked analysis script in `.work/sessions/<session>/analysis/`.
- Existing Chemistries and their tests are unchanged and still pass.

## Architecture & Approach

- Name `Tonneau2025`, files `src/chemistry/catalog/Tonneau2025Reactions.{hh,cc}`, registered in `BuiltInChemistries.cc`; glossary entries **Tonneau2025** and **Extra molecule** are already in `CONTEXT.md`.
- Reactions: all 73 of Table 2 with the paper's rate constants. Where `PureWater` has the same pair, copy its reaction type (0 or 1); for other pairs take the type from Geant4 stock tables. Document the choice per reaction in the file header. The rates of R20 to R37 must be checked visually against the PDF, since text extraction scrambles them.
- Bulk list: the acid-base block (R56 to R73, including H2O pseudo-first-order reactions) driven by `/chem/env/pH`, plus the dissolved-O2 reactions R7, R31 and R47 so `/chem/env/scavenger O2` works. The paper's dose-driven pH drift is not reproduced because the bulk pool is fixed; stated in the file header.
- HO3: add an optional field `constructMolecules` (defaulted to null) to `ChemistryRegistry::Chemistry`; `DnaChemistryList::ConstructMolecule` calls it for the selected Chemistry. `Tonneau2025` uses it to create HO3 (D from a sourced literature value shown to the user for approval; vdW radius from O3, 0.20 nm, as a documented analogue). Other Chemistries are untouched. ADR 0007 records the mechanism and the relaxation of ADR 0002.
- New example macro modelled on `beam_boscolo.in` selecting `Tonneau2025`; docs updated (CLAUDE.md file list as needed).

## Assumptions & Trade-offs

- Earlier stages come from the shared physics, pre-chemistry and dissociation channels; this work adds reaction content only.
- Not reproduced: the paper's ODE solver, dose-rate G-factor and pH drift; no full ODE cross-check (rejected as too large).
- O3 and O3m come from the shared set with Geant4's own parameters.
- HDF5 spatial-snapshot files from `Tonneau2025` have an extra HO3 column and cannot be appended to files from other Chemistries.

## Open Questions

- Literature diffusion coefficient for HO3 (first search was inconclusive; leads: MPEXS2.1-DNA paper, hybrid continuum/Monte Carlo paper arXiv 2601.02132); the user approves the sourced value before it is hardcoded.
- Table 2 rates for R20 to R37 (and the multi-value rows) need visual confirmation from the PDF.

## Notes

- A subagent report established that outputs and the mesoscopic stage enumerate species dynamically, so HO3 appears automatically once created in `ConstructMolecule`, which runs after `/chem/select` is final.
- HO3 needs a nonzero D and a vdW radius for type-1 reactions.
- Template tests: `test/ChemistryRegistryTest.cc` (extend for the null-default `constructMolecules`), `test/ScavengerSpecTest.cc`; a new Geant4-linked test is needed for the HO3 molecule data.
- User chose ship mode: subagent + inline follow-up.
