# Ticket 10: recipe-add-a-per-run-counter

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/add-a-per-run-counter.md && grep -q "Run::Merge" docs/recipes/add-a-per-run-counter.md && grep -q "ReactionCounter" docs/recipes/add-a-per-run-counter.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote this ticket's recipe as the canonical, fullest `Run::Merge` explanation, per my dispatch note carrying forward the merge-timing correction from ticket 02's review (and already applied to tickets 03/04). It independently re-verified the precise timing against `RunAction.cc:92-98` and explicitly calls out and corrects the imprecise "merge at EndOfRunAction when IsMaster()" phrasing that was in this ticket's own spec text, rather than silently matching it. It also confirmed the already-shipped `create-a-scorer.md`/`create-a-species-counter.md` (which link here) are consistent with what it wrote, so no follow-up edits to those files are needed — this closes out the Serial-vs-MT consistency chain across tickets 2, 3, 4 and 10. Also covers the full `RunAccumulator`/`RunAccumulatorMessenger` wiring chain and the `G4UnitDefinition::GetValueOf()` pitfall, both verified against source. No changes needed.

## Blockers / Challenges

None. This was the last of the 10 planned recipe tickets.

## Commits

- 0d9c96c docs: add-a-per-run-counter recipe (ticket 10)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
