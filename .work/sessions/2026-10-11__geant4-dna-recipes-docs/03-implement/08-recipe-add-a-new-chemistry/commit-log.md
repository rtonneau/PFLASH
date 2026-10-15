# Ticket 08: recipe-add-a-new-chemistry

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/add-a-new-chemistry.md && grep -q "BuiltInChemistries" docs/recipes/add-a-new-chemistry.md && grep -q "ChemistryRegistry" docs/recipes/add-a-new-chemistry.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/add-a-new-chemistry.md`, verified against `ChemistryRegistry.cc/.hh`, `BuiltInChemistries.cc/.hh`, `PureWaterReactions.{hh,cc}`, `BoscoloChemReactions.hh` and `ChemistryTypes.hh`. For the Serial-vs-MT question it genuinely traced the Geant4 kernel (not assumed): confirmed `PhysicsList`/`DnaChemistryList` are constructed exactly once per process (`sim.cc:144`, `PhysicsList.cc:15-21`) and that `G4MTRunManagerKernel::StartThread`/`G4TaskRunManagerKernel::StartThread` hand the *same* pointer to every worker rather than rebuilding it, citing the exact kernel source lines.

This surfaced a real, verified contradiction with the already-committed `create-a-macro-command.md` (ticket 06), which had claimed "`DnaChemistryList`/`PhysicsList` are built per-thread" — that was wrong, and I independently re-verified the correction myself (`sim.cc:144`, `PhysicsList.cc:20`) before fixing it. I amended `create-a-macro-command.md` in place (not part of this ticket's own Files to Touch) to state the now-confirmed fact and link to this recipe's fuller, source-traced proof, then re-ran ticket 06's own verification command to confirm it still passes. This fix is committed separately from this ticket's commit, immediately after it, since it touches a different ticket's file.

## Blockers / Challenges

None — caught and fixed a cross-recipe inconsistency before it could ship.

## Commits

- 51e68ca docs: add-a-new-chemistry recipe (ticket 08)

## Time Spent

6m (ticket-start.js to ticket-complete.js)
