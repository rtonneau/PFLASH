# Ticket 04: recipe-mesoscopic-3d-species-counting

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/mesoscopic-3d-species-counting.md` documents the mesoscopic-stage pattern: the hand-over from the particle-based stage to the per-thread `G4DNAEventScheduler` in `src/chemistry/TimeStepAction.cc` (verified against its actual source, including the `G4ThreadLocal` usage at `src/chemistry/TimeStepAction.cc:358`), the cell mesh sizing via `MesoSettings`/`MesoMessenger`, and counting into `MesoSpeciesCounter` (`header/scoring/MesoSpeciesCounter.hh`).
- [ ] Documents export: `SpeciesMeso.Txt`/`SpeciesMeso.csv` via `MesoSpeciesCounter::WriteAscii`/`WriteCsv`, cross-referencing `docs/output/README.md` instead of repeating column semantics.
- [ ] Includes a `## Serial vs MT` section verified against source: `TimeStepAction` and its scheduler are per-thread (`G4ThreadLocal`), `MesoSpeciesCounter` is merged once on the master via `Run::Merge` (`src/actions/Run.cc:108`), same family as ticket 2/3 but confirm the `G4ThreadLocal` detail specifically for this recipe.
- [ ] Links `docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md` instead of repeating its design rationale.

**Files to Touch:**
- `docs/recipes/mesoscopic-3d-species-counting.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/mesoscopic-3d-species-counting.md && grep -q "G4DNAEventScheduler" docs/recipes/mesoscopic-3d-species-counting.md && grep -q "MesoSpeciesCounter" docs/recipes/mesoscopic-3d-species-counting.md && echo OK
```

Expected:
`OK`

**Notes:**

"3D" here means the spatial cell mesh the mesoscopic stage runs on (not a visualization) — be explicit that this recipe is about aggregate per-cell species counts over time, and point to ticket 5 for the separate per-event spatial snapshot (`SpeciesMesoSpatial.h5`) rather than conflating the two.
