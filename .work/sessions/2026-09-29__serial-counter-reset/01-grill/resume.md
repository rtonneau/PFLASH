# Session: serial-counter-reset

**Date:** 2026-09-29T17:00:05.755Z
**Status:** Grill phase complete

## Problem Statement

F-001 (docs/reviews/2026-09-29-whole-project-review.md): in Serial mode (the default), `Run::Run()` on the master finds the live `TimeStepAction`/`SteppingAction` and points `fReactionCounter`/`fInteractionCounter` at their persistent counters. Nothing ever clears them (the only `Clear()` is in `Run::Merge`, MT only), and `RunAction::EndOfRunAction` folds them into `RunAccumulator` after every `/run/beamOn`. So each run re-adds all earlier runs to `Reactions.*` and `PhysicsInteractions.*`, and later dumps keep data from the start of the process. MT is correct; species and energy deposit are unaffected.

## Context & Constraints

- MT: worker `Run` points at the worker's live counters, cleared in `Run::Merge` after merge; master `Run` falls back to its owned (fresh-per-run) counters.
- `GenerateRun` (hence `Run::Run()` and its lookup) happens right before `BeginOfRunAction` on each thread.
- A worker's `Run::Merge` completes before that worker's next `BeginOfRunAction`.
- Bounded work: implemented on the current branch, no plan/tickets.

## Success Metrics

- Serial: macro `beamOn N`, dump `A_`, `beamOn N`, dump `B_` -> `B_` totals of Reactions and PhysicsInteractions ~ `A_` (statistical spread), not ~2x.
- Same result with `--threads 2`.
- Existing unit tests still pass.

## Architecture & Approach

- `RunAction::BeginOfRunAction` clears, on every thread, the counters the new Run already points at: `static_cast<const Run*>(run)->GetReactionCounter()->Clear()` and `GetInteractionCounter()->Clear()`. Serial master / MT workers: live action counters. MT master: Run's owned counters (already empty, harmless).
- Remove the two `Clear()` calls from `Run::Merge`: exactly one reset per run, one place.
- Update stale comments in `Run.cc`/`Run.hh` about the reset.
- The cast-based action lookup stays only in `Run::Run()` (no duplication).

## Assumptions & Trade-offs

- Chose clear-at-BeginOfRun over Run-owned counters (larger change in both actions) and clear-after-accumulate (two reset sites, skipped by the 0-event early return).
- Chose lookup via Run's existing pointers over passing actions into the RunAction constructor.
- Verification by smoke macro only (scratch dir), no unit test: the change is in Geant4-kernel-facing code.

## Open Questions

None.

## Notes

- Q2 from the review (Reactions.* counts bimolecular reactions only, not bulk reactions): intended, no change.
- F-006 (aborted events still counted in PhysicsInteractions) stays in the separate `primary-killer-controls` idea.
- Use enough events for meaningful reaction counts in the smoke comparison (more than 2 if counts are low).

## Token Usage

- **Input:** 26
- **Output:** 8515
- **Cache read:** 1016702
- **Cache creation:** 21964
- **Total:** 1047207
