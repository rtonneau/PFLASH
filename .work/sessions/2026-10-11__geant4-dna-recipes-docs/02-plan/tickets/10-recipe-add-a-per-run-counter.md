# Ticket 10: recipe-add-a-per-run-counter

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/add-a-per-run-counter.md` documents the portable-counter convention used across this project — stream-only I/O, `Add`/`Record`/`Merge`/`Clear`/`WriteAscii`/`WriteCsv` — verified against at least two actual examples: `src/scoring/ReactionCounter.cc` and `header/scoring/TrackLengthTable.hh`/`src/scoring/TrackLengthTable.cc`.
- [ ] Documents how a new counter gets wired in: owned by `RunAccumulator` (or the per-thread `Run`), fed from an action class (e.g. `TrackingAction`/`SteppingAction`), merged via `Run::Merge` (`src/actions/Run.cc:84-111`), and flushed by `RunAccumulatorMessenger`'s dump into the manifest.
- [ ] Includes a `## Serial vs MT` section stating the `Run::Merge` mechanism precisely (per-thread instance on the worker's `Run`, combined into the master `Run` once per `/run/beamOn` at `EndOfRunAction` when `IsMaster()`) — this is the clearest, most-verified instance of the pattern in the codebase, so write it fully here and have tickets 2-4 link to it instead of repeating it.
- [ ] Notes the `G4UnitDefinition::GetValueOf()` memory-corruption pitfall from a Debug test binary linked against a differently-built Geant4 install, per the comment in `src/scoring/ReactionCounter.cc`.

**Files to Touch:**
- `docs/recipes/add-a-per-run-counter.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/add-a-per-run-counter.md && grep -q "Run::Merge" docs/recipes/add-a-per-run-counter.md && grep -q "ReactionCounter" docs/recipes/add-a-per-run-counter.md && echo OK
```

Expected:
`OK`

**Notes:**

Write this ticket's Serial-vs-MT section as the canonical, fullest explanation of `Run::Merge` — tickets 2, 3, 4 should link here for the mechanism rather than re-deriving it, so go back and check those four files are consistent once this one is written (order tickets 2-4 before 10, or reconcile afterward if 10 ships first).
