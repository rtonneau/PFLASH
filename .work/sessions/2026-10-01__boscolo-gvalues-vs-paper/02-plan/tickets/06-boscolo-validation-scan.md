# Ticket 06: boscolo-validation-scan

**Model:** inherit

**Acceptance Criteria:**
- [ ] Scan BoscoloChem, 100 keV, 2 events, pO2 0, 0.5, 5, 10, 21, with `-RateAware -MaxTimeStep '1 ns'` and the log-spaced `-ReactionBins` from ticket 5, into `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/` (at most 3 sims in parallel).
- [ ] `analysis/compare_boscolo_fig3.py` runs on the 5 dumps. The 4 shape checks (H(0%) non-increasing, O2m/HO2(21%) saturated, HO2(0%) ≈ 0) must PASS. If any fails, report BLOCKED with the table and the 21% reaction counts per bin, and change nothing.
- [ ] No end-of-chemistry burst: in the 21% `Reactions.Txt`, the 300 ns–1 µs bin's count for `H3O^1 + O_2^-1 -> HO_2°^0` is ≤ 3× the previous bin's, scaled by the bin-width ratio (3.5).
- [ ] `macro/beam_boscolo.in` gains `/chem/sbs/rateAwareReactions true` and `/chem/sbs/maxTimeStep 1 ns` before `/run/initialize`, with a comment pointing to ADR 0006. CLAUDE.md's description of `beam_boscolo.in` is updated.
- [ ] `validation.md` in the session scratch dir records: the full pass/fail table, the ±10% values (ours vs paper, % deviation), the wall time per level (from Manifest `runs[].wallTime_s`) and the PNG path. Ticket 7 reads it.

**Files to Touch:**
- `macro/beam_boscolo.in`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
python analysis/compare_boscolo_fig3.py .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/o2_0pct .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/o2_0p5pct .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/o2_5pct .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/o2_10pct .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/o2_21pct --out .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t06/fig3_compare.png
```

Expected:
All 4 shape checks PASS. The ±10% rows are PASS or FAIL; record them either way.

**Notes:**

Run `test_run.ps1` three times in the background (e.g. `-Levels 0,0.5`, `-Levels 5,10`, `-Levels 21`), since each call runs its levels sequentially. Its folder naming is `o2_<x>pct` with `.` -> `p`. The ±10% checks don't gate this ticket; ticket 7 handles e_aq/O2⁻ misses.
