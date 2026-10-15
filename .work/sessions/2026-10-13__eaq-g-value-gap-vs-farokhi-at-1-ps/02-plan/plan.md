# Implementation Plan

**Session:** eaq G-value gap vs Farokhi at 1 ps
**Date:** 2026-10-13T19:53:23.155Z
**Estimated effort:** 2-3 days

## Strategy

Measure before changing anything. First build a reference from the unmodified stock scavenger example. Then isolate each candidate cause with one-factor runs in dnachem-min: the solvation subtype (macro only) and the dissociation channel set (needs a per-Chemistry hook so the stock channels can be selected). Compare everything in one analysis script, then apply the fix the evidence supports and document it.

## Tickets Overview

- **Ticket 1:** Build and run the stock Geant4 11.4.1 scavenger example (500 keV e-, eLossMin 12.5 keV, 250 events, 0 % O2) in a scratch directory and extract e_aq G(t).
- **Ticket 2:** Run dnachem-min Farokhi2023 with Meesungnoen2002 instead of Ritchie1994 and measure the effect on e_aq G at 1 ps.
- **Ticket 3:** Add a per-Chemistry dissociation-channel hook (ADR plus test) and a Farokhi2023 channel set copied from the stock example, selectable for experiments.
- **Ticket 4:** Add an analysis script comparing e_aq G(t) for stock, ours, each single-factor variant, and the digitized Farokhi curve; write the findings.
- **Ticket 5:** Apply the supported fix to the Farokhi2023 macros and Chemistry defaults, regenerate the comparison table and update the docs.

## Sequencing Rationale

Ticket 1 gives the target that every later number is judged against. Tickets 2 and 3 are independent experiments; 3 is the larger change so it comes after the cheap macro test. Ticket 4 needs the data from 1-3. Ticket 5 depends on the conclusion of 4 and may shrink to a no-code report if neither candidate explains the gap.

## Risks & Mitigation

- **Risk:** The effect of each candidate is smaller than the Monte Carlo error at 250 events. → **Mitigation:** Use the same fixed seed and the sumG/sumG2 SEM already in the output; raise the event count for the 1 ps point if needed.
- **Risk:** The stock example does not build with the local MSVC setup. → **Mitigation:** Follow `.claude/geant4-instructions.md` for the MSVC environment; build in scratch, outside the repo.
- **Risk:** A new dissociation hook disturbs other Chemistries. → **Mitigation:** The default stays option1; a test asserts other Chemistries are unchanged.

## Assumptions

- The stock example run on Geant4 11.4.1 is a valid proxy for the paper's configuration.
- The 1 % target applies against the stock run; the digitized curve limits comparison to about 3.5 %.
