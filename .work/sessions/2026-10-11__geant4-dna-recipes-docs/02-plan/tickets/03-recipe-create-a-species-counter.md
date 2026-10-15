# Ticket 03: recipe-create-a-species-counter

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/create-a-species-counter.md` documents the Geant4-DNA-specific layer for counting molecular species: how `ScoreSpecies` (`header/scoring/ScoreSpecies.hh`, `src/scoring/ScoreSpecies.cc`) hooks into the chemistry stage to count species per time bin, verified against its actual source (not guessed), and how its output is exported (`Species.Txt`/`Species_nt_species.csv` via `RunAccumulatorMessenger`'s dump, per `docs/output/README.md` and the `sim-output` skill — link, don't repeat).
- [ ] Explicitly defers the generic `G4VPrimitiveScorer` mechanics (registration, `ProcessHits`/`EndOfEvent`, MT merge via `AbsorbResultsFromWorkerScorer`) to `create-a-scorer.md` with a link, rather than repeating them.
- [ ] Includes a `## Serial vs MT` section: same `Run::Merge`/`AbsorbResultsFromWorkerScorer` mechanism as ticket 2 (per-thread `ScoreSpecies` instance per worker, merged once on the master), stated briefly with a link to the fuller explanation in ticket 2's recipe rather than re-deriving it.

**Files to Touch:**
- `docs/recipes/create-a-species-counter.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/create-a-species-counter.md && grep -q "ScoreSpecies" docs/recipes/create-a-species-counter.md && grep -q "create-a-scorer.md" docs/recipes/create-a-species-counter.md && echo OK
```

Expected:
`OK`

**Notes:**

Read `src/scoring/ScoreSpecies.cc` in full before writing this; do not infer the species-counting hook from the class name alone.
