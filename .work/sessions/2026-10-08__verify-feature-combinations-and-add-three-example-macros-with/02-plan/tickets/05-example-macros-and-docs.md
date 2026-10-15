# Ticket 05: example-macros-and-docs

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 0.99)
**Effort:** low

**Acceptance Criteria:**
- [ ] `macro/example_sbs.in`, `example_irt.in`, `example_meso.in`: 10 keV e-, O2 21 % (one commented-out line for pure water), two sub-runs of 2 events with different `/random/setSeeds`, `/run/dumpDataAndResetToDir sub_01` and `sub_02`, short enough for a smoke run, header comments naming the mode.
- [ ] ADR `docs/adr/0008-selectable-chemistry-modes.md` supersedes the "SBS removed" part of ADR 0006 (and ADR 0006 gets a pointer).
- [ ] `CLAUDE.md`, `README.md` and `.claude/skills/sim-output/SKILL.md` describe the modes, `/chem/meso/enable`, the macro lookup, the default results dir and the results index.
- [ ] The three macros run to completion.

**Files to Touch:**
- `macro/example_sbs.in`
- `macro/example_irt.in`
- `macro/example_meso.in`
- `docs/adr/0008-selectable-chemistry-modes.md`
- `docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md`
- `CLAUDE.md`
- `README.md`
- `.claude/skills/sim-output/SKILL.md`

**Verification Step:**

Run:
```bash
cd build && for m in sbs irt meso; do ./sim example_$m.in --dir results_$m || echo FAIL $m; done; ls results_*/
```

Expected:
No FAIL; each results dir holds `sub_01/`, `sub_02/` and an index `Manifest.json`.

**Notes:**

Remove the stray `macro/_t.in`. Use `beam_o2.in` and `beam_boscolo.in` as style references. `CLAUDE.md` still says SBS is unsupported and that `/process/chem/TimeStepModel` must not be in macros; fix both statements.
