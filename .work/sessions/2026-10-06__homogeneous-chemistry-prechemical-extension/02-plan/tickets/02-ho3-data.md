# Ticket 02: ho3-data

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] A note `docs/literature/ho3-parameters.md` gives a diffusion coefficient for HO3 with its source (reference, table or value as printed) found from the paper's references, the Geant4-DNA UHDR example, Buxton et al., or the MPEXS2.1-DNA and arXiv 2601.02132 papers.
- [ ] The note states the vdW radius taken from Geant4's O3 (0.20 nm), reads it from the Geant4 source (`G4O3.cc` and `G4ChemDissociationChannels_option1.cc`), and says it is an analogue.
- [ ] If no credible source is found, the ticket stops with BLOCKED and a list of what was searched; no value is invented.

**Files to Touch:**
- `docs/literature/ho3-parameters.md`

**Verification Step:**

Run:
```bash
grep -n -E "HO3|m2/s|m\^2/s|source" docs/literature/ho3-parameters.md
```

Expected:
Lines naming HO3, a numeric diffusion coefficient with its unit, and a cited source.

**Notes:**

Per the grill, the user approves the sourced value; under `/gps auto` this means the value and source appear in the commit message and the final report so they can be checked before merging. The Geant4 source root is `C:\Users\rtonneau\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1` (not `$DEV_DIR/GEANT4/geant4-v11.4.1-source/source`).
