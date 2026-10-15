# Ticket 04: eaq-comparison-analysis

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] A script plots and tabulates e_aq G(t), 1 ps to 1 ns, for stock, ours, the Meesungnoen2002 variant, the Farokhi-channels variant, and the digitized Farokhi curve.
- [ ] A markdown note states G at 1 ps with SEM for each, the difference to the stock run, and which factor explains the gap.
- [ ] The Farokhi-channels variant has been run (macro plus 250 events).

**Files to Touch:**
- `docs/literature-comparison/farokhi2023/eaq_1ps_gap.py`
- `docs/literature-comparison/farokhi2023/eaq_1ps_gap.md`
- `macro/beam_farokhi_electron_o2_0pct_farokhi_channels.in`

**Verification Step:**

Run:
```bash
python docs/literature-comparison/farokhi2023/eaq_1ps_gap.py
```

Expected:
Writes the PNG and markdown table and prints one G(1 ps) line per source.

**Notes:**

Reuse the SEM estimator from `plot_gvalues.py` (mean = sumG/n, Bessel-corrected). The 1 % target is judged against the stock run, the digitized curve only to its resolution.
