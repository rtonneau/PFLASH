# Ticket 04: recipe-mesoscopic-3d-species-counting

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/mesoscopic-3d-species-counting.md && grep -q "G4DNAEventScheduler" docs/recipes/mesoscopic-3d-species-counting.md && grep -q "MesoSpeciesCounter" docs/recipes/mesoscopic-3d-species-counting.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/mesoscopic-3d-species-counting.md`, verified against `TimeStepAction.cc` (hand-over, `MeshTotalsAction`, the `G4ThreadLocal` at line 358, `CollectMesoSpecies`), `MesoSettings.cc`/`MesoMessenger.cc`, and `Run.cc` (binding and merge). Correctly distinguishes aggregate per-cell counting (this recipe) from the per-event spatial snapshot (deferred to ticket 5, linked even though that file doesn't exist yet — it's already indexed in README.md so the link target is expected to appear). Carries forward the merge-timing correction from tickets 02/03. Links ADR 0006/0008 for rationale instead of repeating. No changes needed.

## Blockers / Challenges

None.

## Commits

- 0a5e1fe docs: mesoscopic-3d-species-counting recipe (ticket 04)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
