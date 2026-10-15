# Ticket 05: apply-fix-and-docs

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] The supported fix is applied: Farokhi macros set the subtype that the analysis supports, and/or Farokhi2023 selects its channel set by default.
- [ ] e_aq at 1 ps in the regenerated runs agrees with the stock run within 1 % or the Monte Carlo error, whichever is larger.
- [ ] `simulation_vs_literature_comparison.csv` and `.md` are regenerated or annotated, and `CLAUDE.md` is updated if the Chemistry behavior changed.
- [ ] If the analysis shows neither factor explains the gap, no code changes; the note says so with the measured residual.

**Files to Touch:**
- `macro/beam_farokhi_electron_*.in`
- `src/chemistry/catalog/Farokhi2023Reactions.cc`
- `docs/literature-comparison/simulation_vs_literature_comparison.md`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
python docs/literature-comparison/farokhi2023/eaq_1ps_gap.py
```

Expected:
The fixed run's G(1 ps) matches the stock value within the stated tolerance.

**Notes:**

Scope depends on the result of ticket 4; do not change shared dissociation channels for other Chemistries.
