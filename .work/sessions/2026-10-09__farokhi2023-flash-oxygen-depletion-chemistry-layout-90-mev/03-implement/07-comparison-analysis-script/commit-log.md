# Ticket 07: comparison-analysis-script

**Status:** ✅ Done

## Local Test Result

Subagent read tickets 5/6's real `Species_nt_species.csv` output directly (no macro re-run) and ran `python compare_farokhi2023.py`: exit 0, wrote both PNGs; visually confirmed both dnachem-min's curves and digitized reference markers present and distinguishable in every panel.

This session re-ran `python compare_farokhi2023.py` independently: exit 0, both PNGs regenerated (`farokhi2023_proton_gvalues.png` 236506 bytes, `farokhi2023_electron_gvalues.png` 148045 bytes). Opened both images directly: the proton figure shows 8 species panels (OH, H3Op, H, e_aq, H2, H2O2, HO2, O2m) each with 4 colored O2-level curves plus black `x` digitized-reference markers tracking the 21% curve; the electron figure shows 4 species panels (e_aq, O2m, H, HO2) with 4 colored curves each, plus matching-colored digitized markers per O2 level. Both show sensible physical trends (faster e_aq/H depletion and faster O2m/HO2 buildup at higher O2%, consistent ordering across panels).

## Review Notes

Read `compare_farokhi2023.py` and `README.md` in full against the Acceptance Criteria: script reads `Species_nt_species.csv` from the 8 real run directories (hardcoded list, matching the Tonneau2025 precedent's pattern); plots the exact species lists the ticket specifies for Fig. 2 (8 species, superset including the 4 the ticket named) and Fig. 5 (e_aq, O2m, H, HO2); reference points are a plain data literal (`REFERENCE_PROTON_21PCT`, `REFERENCE_ELECTRON`), explicitly labeled hand-digitized/approximate in both the module docstring and README, with the exact caveat paragraph the ticket requires (single-box geometry approximation, `G4EmDNAPhysics_option2` vs. unstated paper option, hand-digitized references) -- matches the Tonneau2025 precedent's style. `sumG / nEvent` (mean G-value per event, matching `ScoreSpecies`'s own accumulation) is a better-justified metric than the ticket's suggested `Species.Txt` + `Manifest.json` ratio-of-sums fallback, and is documented as such; accepted this as a reasonable interpretation that still satisfies the acceptance criteria's intent (correct G-value vs. time per species). No source/project files touched, only the session's own analysis folder. All acceptance criteria hold.

## Blockers / Challenges

None. One minor note: the subagent installed `pymupdf` via pip to render PDF pages for visual reference-point reading (no `poppler`/ghostscript on this machine) -- a one-off, reversible dev-environment side effect for this session's analysis work, not a project dependency change; nothing in `src/`, `CMakeLists.txt`, or any requirements file was touched.

## Commits

- fb77d5a chore: Farokhi2023 G-value comparison script (ticket 07)

## Time Spent

10m (ticket-start.js to ticket-complete.js)
