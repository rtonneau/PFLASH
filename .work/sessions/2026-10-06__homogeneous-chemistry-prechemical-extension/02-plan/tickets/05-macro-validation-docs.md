# Ticket 05: macro-validation-docs

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `macro/beam_tonneau2025.in` selects `Tonneau2025` before `/run/initialize`, uses a 10 keV electron gun and `/run/beamOn 2`, modelled on `macro/beam_boscolo.in`.
- [ ] An analysis script in `.work/sessions/2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/` (force-added to git) compares species yields at about 100 ns with the paper's Table 3 G-values for e_aq, H3Op, OH, H, H2 and H2O2, and prints the ratio per species with the paper values quoted.
- [ ] The comparison result is recorded in the commit message or a short note beside the script, including any species that deviate strongly.
- [ ] `CLAUDE.md` (Key Files, Macro and Logging example macros, Source layout if relevant) and `docs/output/` are updated where the Chemistry or the HO3 column matters; the spatial-snapshot caveat about mixing files from different Chemistries is documented.

**Files to Touch:**
- `macro/beam_tonneau2025.in`
- `.work/sessions/2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/compare_gvalues.py`
- `CLAUDE.md`
- `docs/output/SpeciesMesoSpatial-h5.md`

**Verification Step:**

Run:
```bash
cd build && ./sim beam_tonneau2025.in --dir ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t5 > ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t5.log 2>&1 && python -I ../.work/sessions/2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/compare_gvalues.py ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t5
```

Expected:
The run exits 0 and the script prints one line per species with the simulated G-value, the paper's G-value and their ratio.

**Notes:**

Table 3 (G-values at 100 ns for LET 1.17 keV/um) is garbled in text extraction; read it from the PDF page images. A 10 keV electron is higher LET than that column, so say in the note that the comparison is indicative, not an exact reproduction. Follow the `sim-output` skill for file formats and the `--dir` flag; build the app from `build/` (RelWithDebInfo) with the MSVC environment.
