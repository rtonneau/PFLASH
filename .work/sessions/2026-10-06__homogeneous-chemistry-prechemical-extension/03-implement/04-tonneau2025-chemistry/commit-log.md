# Ticket 04: tonneau2025-chemistry

**Status:** ✅ Done

## Local Test Result

Subagent ran the ticket's smoke run: exit 0, both success markers, no EEEE, only the known WrongResolution warning; `HO3^0` is a column in SpeciesMesoSpatial.h5 (grep 1; text species files show 0 because HO3 never forms in 2 pure-water events, O3 only arising via R15). Session rebuilt all targets and re-ran `ctest --test-dir build-ninja`: 14/14 passed.

## Review Notes

Implemented by an opus/high subagent; reviewed against the report and the status of the tree. All criteria hold; the HO3 criterion is met through the HDF5 column (ticket 05's macro enables spatial output). The subagent fixed a ticket 03 bug outside this ticket's file list: the hook ran when PhysicsList was constructed, before any macro, so /chem/select now also calls it (ChemistrySelectMessenger.cc, ADR 0007 paragraph). Reaction-type exceptions R13 and R30 (type 0, k above the diffusion limit) are documented in the header. OPEN, flagged for the user: R57, R66, R69, R73 are kept as s^-1 (ticket 01's reading). Detailed balance checked by the session (R57: 1.33e10 x Kw/Ka(HO2) / 55.5 = 0.19 vs 0.155; R69: 1.27e6 x 55.5 / 1.33e10 vs Ka/Kw) supports M^-1 s^-1 times 55.5 M, which would raise e_aq + H2O from 19 to about 1050 s^-1; G4EmDNAChemistry_option3 uses the s^-1 reading. Not changed pending the user's decision.

## Blockers / Challenges

Ticket 03 hook timing bug (fixed here). Geant4 fatal CONF_ALREADY_RECORDED if HO3 is created with CreateConfiguration; created as the definition's default configuration instead. G4DNAScavengerProcess keeps one reaction per (molecule, partner), so R23 is absent from the particle-based stage and R15/R16 share a pair; both documented.

## Commits

- 00b2024 feat: Tonneau2025 Chemistry with HO3 extra molecule; /chem/select runs the molecule hook (ticket 04)

## Time Spent

26m (ticket-start.js to ticket-complete.js)
