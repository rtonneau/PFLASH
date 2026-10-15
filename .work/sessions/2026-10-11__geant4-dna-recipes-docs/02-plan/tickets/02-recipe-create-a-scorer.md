# Ticket 02: recipe-create-a-scorer

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/create-a-scorer.md` documents the generic Geant4 `G4VPrimitiveScorer` pattern: subclassing, the `ProcessHits`/`EndOfEvent` lifecycle, and registration via `G4MultiFunctionalDetector::RegisterPrimitive`, each claim verified against Geant4 source (`geant4SourcePath` or the KB) with a citation, not guessed.
- [ ] Cites dnachem-min's own registration site as the reference implementation: `src/geometry/DetectorConstruction.cc:124-146` (`ConstructSDandField`, registering `PrimaryKiller` and `ScoreSpecies` on the same `G4MultiFunctionalDetector`).
- [ ] Includes a `## Serial vs MT` section stating the MT merge mechanism verified against source: per-thread scorer instance, combined on the master via `AbsorbResultsFromWorkerScorer` inside `Run::Merge` (`src/actions/Run.cc:84-111`, called from `RunAction::EndOfRunAction` when `IsMaster()`), vs. a single instance and no merge step in Serial mode.
- [ ] Links `docs/adr/0003-clustered-source-layout.md` (where a new scorer file belongs) instead of repeating its content.

**Files to Touch:**
- `docs/recipes/create-a-scorer.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/create-a-scorer.md && grep -q "RegisterPrimitive" docs/recipes/create-a-scorer.md && grep -q "AbsorbResultsFromWorkerScorer" docs/recipes/create-a-scorer.md && echo OK
```

Expected:
`OK`

**Notes:**

Keep this recipe about the generic Geant4 scorer mechanics only; the Geant4-DNA-specific layer (how `ScoreSpecies` actually counts species) belongs to ticket 3, which should link back here rather than repeat this content.
