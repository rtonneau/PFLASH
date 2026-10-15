# Session: eaq G-value gap vs Farokhi at 1 ps

**Date:** 2026-10-13T19:47:32.818Z
**Status:** Grill phase complete

## Problem Statement

For 500 keV electrons at 0 % O2, e_aq G at 1 ps is 4.04 molecules/100 eV in dnachem-min (both IRT_syn and SBS), against about 4.21 in the digitized Farokhi et al. 2023 Fig. 5 (about 4 % higher) and about 4.49 in Boscolo 2020 (a different code, TRAX-CHEM). Farokhi used Geant4-DNA with a similar configuration, so the gap is unexpected. Find the origin of the difference and, if it is ours, fix it.

## Context & Constraints

- **Current behavior:** Farokhi2023 Chemistry runs use `G4EmDNAPhysics_option2`, `G4ChemDissociationChannels_option1` (shared by all Chemistries) and `/process/dna/e-SolvationSubType Ritchie1994`. Digitization error of Farokhi Fig. 5 is about 0.1-0.2 G units (about 3.5 %).
- **Pain point:** Farokhi built on the stock Geant4 `scavenger` example, which defines its own dissociation channels in `EmDNAChemistry.cc` (B1A1 excitation: autoionisation 0.55, relaxation 0.30, dissociative 0.15; no visible dissociative-attachment or vibrational-decay channels in the part read) and whose `beam.in` sets solvation `Meesungnoen2002`. Ours: B1A1 autoionisation 0.50 split over 5 channels, plus dissociative-attachment and vibrational-decay channels.
- **Dependencies:** Dissociation channels and molecules are shared by all Chemistries today (ADR 0007 gives a per-Chemistry hook for Extra molecules only). Data: `docs/literature-comparison/simulation_vs_literature_comparison.csv`, `farokhi2023/`, Farokhi macros in `macro/beam_farokhi_electron_*`. The stock example lives at `${DEV_DIR}/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1/examples/extended/medical/dna/scavenger`.
- **Tech stack:** Geant4 11.4.1, C++20, CMake, MSVC, Python analysis scripts.

## Success Metrics

- A fresh run of the unmodified stock scavenger example (500 keV e-, eLossMin 12.5 keV, 250 events, 0 % O2) gives a reference e_aq G(t) from 1 ps to 1 ns.
- The cause(s) of the 4.04 vs about 4.21 gap are identified with evidence, each candidate (solvation subtype, dissociation scheme) isolated by a single-factor run.
- After the fix, dnachem-min Farokhi2023 e_aq at 1 ps agrees with the stock run to 1 % or to the Monte Carlo error, whichever is larger, and with the digitized curve to within its resolution (about 3.5 %); the curve shape to 1 ns improves.
- If neither candidate explains the gap, this is reported with the measured residual and no code change is made.

## Architecture & Approach

1. Build the stock scavenger example in a scratch directory outside the repo and run it with the same beam, events and eLossMin.
2. Run controlled dnachem-min variants that change one factor at a time: solvation Meesungnoen2002 vs Ritchie1994, and the example's dissociation channel set vs option1.
3. Compare e_aq G at 1 ps and the curve to 1 ns against the stock run and the digitized Fig. 5 (analysis script in `docs/literature-comparison/`).
4. Fix: if the solvation subtype is the cause, change the Farokhi macros. If the dissociation scheme is the cause, add a per-Chemistry dissociation-channel hook (like ADR 0007) so Farokhi2023 selects the example's channel set while other Chemistries keep option1; write a new ADR and tests.

## Assumptions & Trade-offs

- Assumes the stock example, run in the same Geant4 11.4.1, is the right proxy for the paper (the paper used 11.0 or later). A Geant4 11.0 build is out of scope.
- The 1 % target applies against the stock run, not the hand-digitized data, whose resolution limits the comparison to about 3.5 %.
- Not doing: UHDR or pulse structure, the proton runs, changing the dissociation scheme of other Chemistries, changing physics code.

## Open Questions

- Does the stock example actually register dissociative-attachment and vibrational-decay channels? To be confirmed in the source while testing.
- The solvation and dissociation effects may be smaller than the Monte Carlo error at 250 events; more events may be needed.

## Notes

- Grill decisions: diagnose then fix; compare against both the digitized curve and a fresh stock run; test solvation subtype and dissociation scheme; per-Chemistry hook allowed if the dissociation scheme is the cause; stock example build approved in a scratch directory.
- Boscolo is a different code (TRAX-CHEM) and is not a target of the fix.
