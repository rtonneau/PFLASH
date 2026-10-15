# Ticket 02: solvation-subtype-test

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] A copy of the Farokhi 0 % O2 electron macro uses `/process/dna/e-SolvationSubType Meesungnoen2002`, otherwise identical.
- [ ] It runs 250 events with the existing seed and gives e_aq G at 1 ps with SEM.
- [ ] The difference to the Ritchie1994 run is recorded in the ticket notes.

**Files to Touch:**
- `macro/beam_farokhi_electron_o2_0pct_meesungnoen.in`

**Verification Step:**

Run:
```bash
grep -n "SolvationSubType" macro/beam_farokhi_electron_o2_0pct_meesungnoen.in
```

Expected:
`/process/dna/e-SolvationSubType Meesungnoen2002`

**Notes:**

Run from `build/` as in `.claude/geant4-instructions.md`. Do not edit the existing Farokhi macros in this ticket.
