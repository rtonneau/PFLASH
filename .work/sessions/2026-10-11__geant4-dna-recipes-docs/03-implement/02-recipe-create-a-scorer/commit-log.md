# Ticket 02: recipe-create-a-scorer

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/create-a-scorer.md && grep -q "RegisterPrimitive" docs/recipes/create-a-scorer.md && grep -q "AbsorbResultsFromWorkerScorer" docs/recipes/create-a-scorer.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/create-a-scorer.md`. Verified against acceptance criteria: documents `G4VPrimitiveScorer`'s `ProcessHits`/`Initialize`/`EndOfEvent` lifecycle and `G4MultiFunctionalDetector::RegisterPrimitive`, each cited against the actual Geant4 source under `geant4SourcePath/source/digits_hits/detector/` (file + line, not guessed); cites `DetectorConstruction.cc:124-146` as the reference registration site; links `docs/adr/0003-clustered-source-layout.md` instead of repeating it. The Serial vs MT section is more precise than my ticket spec's phrasing: it correctly states that `G4MTRunManager::MergeRun` (kernel, `source/run/src/G4MTRunManager.cc`) invokes `Run::Merge` *before* `RunAction::EndOfRunAction` runs on the master — not that `EndOfRunAction` itself triggers the merge, which is what my ticket text implied. Confirmed this against `src/actions/RunAction.cc:92-98`'s own comment ("fed by Run::Merge -> AbsorbResultsFromWorkerScorer"), which only makes sense if the merge already happened by the time `EndOfRunAction` reads the master run. No changes needed; this correction should be reflected in tickets 3, 4, 10's recipes rather than my less precise ticket wording.

## Blockers / Challenges

None.

## Commits

- d65e436 docs: create-a-scorer recipe (ticket 02)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
