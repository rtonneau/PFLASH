# Implementation Plan

**Session:** homogeneous-chemistry-prechemical-extension
**Date:** 2026-10-06T17:28:01.332Z
**Estimated effort:** 1 day

## Strategy

Build the Chemistry from the bottom up: first the Table 2 data as plain, Geant4-free data that a unit test can check, then the HO3 molecule data, then the registry hook that lets a Chemistry add molecules, then the Chemistry itself wired to both, and last the example macro, the 100 ns G-value check and the docs. Each ticket builds and passes tests on its own.

## Tickets Overview

- **Ticket 1:** Transcribe Table 2 (73 reactions, bulk list) into a std-only data table, with the R20 to R37 rates checked against the PDF, plus a unit test of the table.
- **Ticket 2:** Source a literature diffusion coefficient for HO3 and record it, with the O3-analogue radius, in a tracked note.
- **Ticket 3:** Add the optional `constructMolecules` field to `ChemistryRegistry::Chemistry`, call it from `DnaChemistryList::ConstructMolecule`, extend the registry test, write ADR 0007.
- **Ticket 4:** Implement the `Tonneau2025` Chemistry (reaction builder, bulk list, HO3 creation) from the table and register it.
- **Ticket 5:** Add the example macro, run the smoke test, compare species yields at about 100 ns with the paper's G-values in a tracked analysis script, and update the project docs.

## Sequencing Rationale

Ticket 1 and 2 produce the data that ticket 4 consumes and need no code changes elsewhere. Ticket 3 is the only change to shared code and is isolated so the existing Chemistries can be re-tested before anything uses it. Ticket 4 needs 1 to 3. Ticket 5 needs a working Chemistry.

## Risks & Mitigation

- **Risk:** Table 2 rates for R20 to R37 are unreadable in text extraction, so a wrong value is transcribed. → **Mitigation:** read the table page images with the Read tool (PDF pages), and have the test pin every k to the verified value.
- **Risk:** no credible literature D for HO3 exists. → **Mitigation:** ticket 2 reports BLOCKED instead of inventing a value, so a human decides.
- **Risk:** the new molecule changes spatial-snapshot species columns. → **Mitigation:** only `Tonneau2025` creates HO3, and the docs state that files from different Chemistries must not be mixed.
- **Risk:** a type-1 reaction on a species without a vdW radius gives a wrong reaction radius. → **Mitigation:** HO3 gets the O3 radius, and ticket 4 checks every species in a type-1 reaction has one.

## Assumptions

- The session branch is created by the plan save; commits go on it, one per ticket.
- Builds follow `.claude/geant4-instructions.md`: tests from `build-ninja/` (Debug), runs from `build/`.
- Reaction types are copied from `PureWater` for shared pairs, and from Geant4 stock tables otherwise; the choice is documented per reaction.
