# Ticket 04: fig3-comparison-tool

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `analysis/reference/boscolo2020_fig3.csv` with columns `species,pO2_pct,time_s,G` (species in `e_aq,H,O2m,HO2`) holds about 10 points per curve, log-spaced from 1e-12 to 1e-6 s, for pO2 0, 0.5, 5, 10, 21, hand-digitized from Fig. 3 of Boscolo et al., Int. J. Mol. Sci. 2020, 21, 424. A `#` header names the source, the figure and the digitization uncertainty (±0.02 G). Spot values must hold: e_aq(0%, 1e-6) ≈ 2.25, O2m(21%, 1e-6) ≈ 2.24, HO2(21%, 1e-6) ≈ 0.66, H(0%, 1e-6) ≈ 0.55, H(all, 1e-12) ≈ 0.82, e_aq(all, 1e-12) ≈ 4.5.
- [ ] `analysis/compare_boscolo_fig3.py <dump-dir> [<dump-dir> ...] [--out <png>]` (Python 3, numpy/pandas/matplotlib) reads each dir's `Species_nt_species.csv` (G = sumG / nEvent; species matched after stripping `^charge`, `°` and any mis-decoded byte: `e_aq`, `H`, `O_2` with charge -1 -> O2m, `HO_2` with charge 0 -> HO2) and `Manifest.json` (pO2 % = O2 `molarity_M` / 0.0013 × 100, 0 if absent; rounded to the nearest reference level).
- [ ] It writes a 2x2 figure (e_aq, H, O2m, HO2; log time axis; ours solid, reference markers; one colour per pO2) and prints a pass/fail table:
  - `H(0%) non-increasing`: for successive output times t ≥ 1 ps, G(t_next) ≤ G(t) + 0.02.
  - `O2m(21%) saturated` and `HO2(21%) saturated`: |G(1 µs) - G(0.5 µs)| / G(1 µs) < 0.05.
  - `HO2(0%) ≈ 0`: G(1 µs) < 0.05.
  - ±10% at 1 µs: O2m(21%) vs 2.24, HO2(21%) vs 0.66, e_aq(0%) vs 2.25, H(0%) vs 0.55.
  - A check whose pO2 level is missing from the inputs prints `SKIP`. The exit code is 0 only when no check FAILs.
- [ ] Run on today's scratch dumps (`runs/o2_0`, `runs/o2_21`), it exits 1, with `H(0%) non-increasing`, `HO2(21%) saturated` and `O2m(21%) ±10%` all FAIL.
- [ ] `analysis/README.md` (short) gives usage and what each check means.

**Files to Touch:**
- `analysis/reference/boscolo2020_fig3.csv`
- `analysis/compare_boscolo_fig3.py`
- `analysis/README.md`

**Verification Step:**

Run:
```bash
python analysis/compare_boscolo_fig3.py .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/runs/o2_0 .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/runs/o2_21 --out .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t04.png; echo exit=$?
```

Expected:
The table shows FAIL for `H(0%) non-increasing`, `HO2(21%) saturated` and `O2m(21%) ±10%`, SKIP for the checks that need missing levels, `exit=1`, and the PNG exists.

**Notes:**

Fig. 3 is rendered at `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/fig3.png`; open it with the Read tool to digitize. Axis ticks: x is log, from 1e-12 to 1e-6 s; y runs 0–4.5 (e_aq, O2m) and 0–0.9 (H, HO2). Fig. 3's colours run blue (0%) to red (21%) over 8 levels: 0, 0.015, 0.5, 1, 2.5, 5, 10, 21; use only 0, 0.5, 5, 10, 21. For better resolution, re-render one panel at a higher dpi with PyMuPDF (`PYTHONPATH=.scratch/tests/2026-09-30__boscolochem-table1/pylib`, `import pymupdf`, page index 4, Windows-style output path). Today's scratch values for sanity: 0%, 1 µs: e_aq 2.03, H 0.85; 21%, 1 µs: O2m 3.14, HO2 0.88. Read the CSV with `encoding='utf-8', errors='replace'` (the `°` byte is mis-decoded).
