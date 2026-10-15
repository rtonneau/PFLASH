# Ticket 06: recipe-create-a-macro-command

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/create-a-macro-command.md && grep -qi "DeclareMethod" docs/recipes/create-a-macro-command.md && grep -qi "PreInit" docs/recipes/create-a-macro-command.md && echo OK` → `OK` (re-ran myself).

## Review Notes

This is the ticket flagged as the highest risk for an unverified kernel-MT claim. The subagent (sonnet) did the real work: traced the PreInit-command-replay mechanism through five actual source files (`G4UImanager::ApplyCommand`'s command-stacking at `G4UImanager.cc:539-553`, `stackCommandsForBroadcast` set by `G4MTRunManager`'s constructor at `G4MTRunManager.cc:146`, `PrepareCommandsStack()` draining the stack at `G4MTRunManager.cc:286-295`/call site `:416`, and the per-worker replay in `G4WorkerRunManager::DoWork` at `G4WorkerRunManager.cc:736-741`, before that worker's own `BeamOn`), each cited by exact file:line, not asserted from general Geant4 familiarity. Also verified both messenger styles, the new-vs-existing-directory guidance (via `G4UImessenger::CreateDirectory`), and the `DeclareMethod` truncation pitfall against `G4GenericMessenger.cc:228-274`'s actual tokenizer loop plus the project's own `DnaChemistryList.cc:170-176` comment and `DnaChemistryList.hh:138-144` doc comment. No changes needed — this recipe met the ticket's bar for "unverified, say so" vs. "verified, cite it" by actually finding the citation.

## Blockers / Challenges

None.

## Commits

- 2981cf2 docs: create-a-macro-command recipe (ticket 06)

## Time Spent

4m (ticket-start.js to ticket-complete.js)
