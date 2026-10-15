# Ticket 02: ho3-data

**Status:** ✅ Done

## Local Test Result

`grep -n -E "HO3|m2/s|source" docs/literature/ho3-parameters.md`: lines naming HO3, D = 2.0e-9 m2/s and the O3 basis.

## Review Notes

The subagent found no literature D for HO3 and reported BLOCKED. The user then approved an analogue (O3, D = 2.0e-9 m2/s, radius 0.20 nm) on 2026-10-06, so criterion 1 is met with that approved analogue rather than a measured value. The note lists everything searched, marks the values as assumptions, and names the single place to change them. O3 values come from the subagent's reading of G4O3.cc and G4ChemDissociationChannels_option1.cc.

## Blockers / Challenges

**Blocked (2026-10-06T17:36:31.107Z):** No literature diffusion coefficient for HO3 found (paper, Geant4 source and examples, arXiv 2601.02132, MPEXS2.1-DNA main text, web); needs a user decision on a source or an analogue D

## Commits

- 89fdac4 docs: HO3 parameters for Tonneau2025, D and radius from O3 analogue (ticket 02)

## Time Spent

36m (ticket-start.js to ticket-complete.js)
