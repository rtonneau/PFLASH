# Ticket 01: stock-scavenger-reference

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] The stock scavenger example builds in a scratch directory outside the repo.
- [ ] It runs 500 keV e-, eLossMin 12.5 keV, 250 events, 0 % O2 with the same seed convention as our runs.
- [ ] e_aq G(t) from 1 ps to 1 ns, with SEM, is saved as a CSV under `docs/literature-comparison/farokhi2023/`.
- [ ] The exact macro and build commands are recorded in a short markdown note.

**Files to Touch:**
- `docs/literature-comparison/farokhi2023/stock_scavenger_reference.md`
- `docs/literature-comparison/farokhi2023/data/stock_scavenger_eaq.csv`

**Verification Step:**

Run:
```bash
head -5 docs/literature-comparison/farokhi2023/data/stock_scavenger_eaq.csv
```

Expected:
A header row and rows starting at about 1e-12 s with an e_aq G near 4 and an SEM column.

**Notes:**

Source: `${DEV_DIR}/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1/examples/extended/medical/dna/scavenger`. Do not modify the example's sources. Its `beam.in` sets Meesungnoen2002; record which subtype the run used. Follow `.claude/geant4-instructions.md` for the MSVC environment.
