# Ticket 03: recipe-create-a-species-counter

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/create-a-species-counter.md && grep -q "ScoreSpecies" docs/recipes/create-a-species-counter.md && grep -q "create-a-scorer.md" docs/recipes/create-a-species-counter.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/create-a-species-counter.md`, grounded in a full read of `ScoreSpecies.cc/.hh`, `ActionInitialization.cc:80-98` and `TimeStepAction.cc:565-577` (not inferred from the class name). It documents the `G4MoleculeCounter` hook, the per-event/per-run reset rationale, the hand-over-time cutoff, and output file pointers (linking `docs/output/README.md`/`sim-output` rather than repeating format details). It defers generic scorer mechanics to `create-a-scorer.md` and correctly carries forward the merge-timing correction from ticket 02 (kernel's `G4MTRunManager::MergeRun` runs before `EndOfRunAction` observes the merged run), linking rather than re-deriving it. No changes needed.

## Blockers / Challenges

None.

## Commits

- 9be2261 docs: create-a-species-counter recipe (ticket 03)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
