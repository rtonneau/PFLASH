# Ticket 07: comparison-analysis-script

**Model:** sonnet
**Effort:** high

**Acceptance Criteria:**
- [ ] A Python script exists under this session's analysis folder (e.g. `.work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/compare_farokhi2023.py`), reading the `Species.Txt`/`.csv` output produced by tickets 5 and 6's macro runs (paths passed as arguments or a small hardcoded list, following the precedent of the existing Tonneau2025 check referenced in CLAUDE.md).
- [ ] The script plots dnachem-min's time-dependent G-values (molecules/100 eV vs. time, log-log or log-linear per the paper's own axes) for the species the paper tracks (`e_aq`, `OH`, `H`, `H3Op`, `H2`, `H2O2`, `O2m`, `HO2` for the proton case per Fig. 2; `e_aq`, `O2m`, `H`, `HO2` for the electron case per Fig. 5), overlaid against a small set of digitized reference points hand-picked from the paper's Fig. 2 (21% O2 panel, for the Fig. 3 TRAX-CHEM comparison) and Fig. 5 (all 4 panels), stored as a plain data literal in the script or a small sidecar CSV — explicitly labeled as hand-digitized/approximate, not extracted via an automated tool.
- [ ] The script's output (PNG or similar, written next to the script) is produced by actually running it against tickets 5 and 6's real macro output, not synthetic/placeholder data.
- [ ] A short paragraph (in the script's module docstring or an adjacent `README.md` in the same analysis folder) states this comparison is indicative only, per the same caveat already used for the Tonneau2025 check (10 keV electrons vs. the paper's 1.17 keV/µm in that case; here, the caveat is the single-box geometry approximation of the paper's nested sensitive volume, the `G4EmDNAPhysics_option2` vs. exact paper physics settings match, and the hand-digitized reference points).

**Files to Touch:**
- `.work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/compare_farokhi2023.py`
- `.work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/README.md`

**Verification Step:**

Run:
```bash
python .work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/compare_farokhi2023.py
```

Expected: exit 0; a PNG (or equivalent image) is written into the same analysis folder; opening it shows dnachem-min's curves alongside the digitized reference points, both visibly present (not a blank or single-series plot).

**Notes:**

This script is session-local, exploratory output — not project infrastructure, not wired into CMake/CTest, not referenced from CLAUDE.md (same precedent as the existing Tonneau2025 indicative check). If tickets 5/6's macro output doesn't exist yet at the paths this script expects, run the relevant macros first (see tickets 5/6's Verification Steps) rather than fabricating sample data.
