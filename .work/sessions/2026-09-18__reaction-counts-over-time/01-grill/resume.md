# Session: reaction-counts-over-time

**Date:** 2026-09-18T15:37:00.000Z
**Status:** Grill phase complete

## Problem Statement

There is no output showing how many times each chemical reaction actually fired over the
course of a simulation run. `Species.Txt` / `Species_nt_species.csv` (from `ScoreSpecies`)
report species *population* at fixed checkpoint times, and the prior `reaction-table-export`
work dumps the static reaction *table* (rate constants, one-time), but neither says how often
each reaction occurred, or when. Add new output data giving the count over time of each
reaction (at least bimolecular reactions).

## Context & Constraints

- **Current behavior:** `TimeStepAction::UserReactionAction(trackA, trackB, products)`
  (`src/TimeStepAction.cc`) is called by Geant4's `G4ITModelProcessor` for every bimolecular
  (track+track) reaction that actually fires during chemistry stepping, but the project's
  implementation is currently an empty/commented-out stub — nothing records these events today.
  Species population is scored separately by `ScoreSpecies` (a `G4VPrimitiveScorer` on the
  `mfDetector` SD), which snapshots `G4MoleculeCounter` at fixed times
  (1,10,100 ps; 1,10,100 ns; ~1 µs) and writes `Species.Txt` + `Species_nt_species(_all).csv`
  via `G4AnalysisManager`, merged across MT worker threads through `Run::Merge` ->
  `ScoreSpecies::AbsorbResultsFromWorkerScorer`, triggered from `RunAction::EndOfRunAction`
  (master only).
- **Pain point:** No artifact exists to see reaction *activity* (counts/rates) over time —
  only static rate constants (`reaction-table-export`'s `/chem/reaction/dump`) or species
  population snapshots. Can't tell which reactions dominate at which point in the chemical
  stage, or sanity-check reaction frequency against expected kinetics.
- **Dependencies:** The shared bimolecular network (pure water + O2, built by
  `PureWaterReactions::BuildPureWaterReactions` into `G4DNAMolecularReactionTable`) registers
  exactly one `G4DNAMolecularReactionData*` per unordered reactant pair (no branching
  channels), confirmed by reading `PureWaterReactions.cc`'s `add()` lambda
  (`reactionTable->SetReaction(rd)`, one call per pair). That pointer, retrieved via
  `G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB)` (same API
  `ReactionTableDump.cc` already uses), is a stable per-run key for "which reaction."
  Verified against Geant4 11.4.1 source
  (`source/processes/electromagnetic/dna/management/src/G4ITModelProcessor.cc:221` calls
  `fpUserTimeStepAction->UserReactionAction(*pTrackA, *pTrackB, productsVector)` for every
  IT/track-track reaction) that this hook fires only for the shared reaction-table network.
  The acid-base/scavenger network (`H3Op(B)`/`OHm(B)` etc., registered via
  `G4DNAScavengerProcess`) resolves entirely inside that process's own `PostStepDoIt`
  (`G4DNAScavengerProcess.cc`) with no reaction-table callback — confirmed by reading that
  file — so it is NOT observable through `UserReactionAction` and is explicitly out of scope.
- **Tech stack:** Geant4-DNA (`G4UserTimeStepAction::UserReactionAction`, `G4Scheduler`,
  `G4DNAMolecularReactionTable`, `G4DNAMolecularReactionData`), project's `Run`/`RunAction`
  worker-merge pattern (`Run::Merge`, `IsMaster()`-gated write in
  `RunAction::EndOfRunAction`), `OutputDir::Resolve` for `--dir`-aware output paths,
  `G4AnalysisManager` for CSV ntuples.

## Success Metrics

- Running a macro (`/run/beamOn N`) produces `Reactions.Txt` (human-readable) and
  `Reactions_nt_reactions.csv`, each showing, for every time bin, each bimolecular reaction
  that fired at least once and how many times, aggregated across all events in the run.
- Counts are read from live per-occurrence callbacks (`UserReactionAction`), not derived from
  rate constants or estimated — an actual tally of what happened during the run.
- Reaction labels match the existing `reaction-table-export` dump's format
  (`A + B -> C + D`), so the two outputs are easy to cross-reference.
- MT-safe: counts recorded on worker threads are correctly merged into the master's output,
  following the same `Run::Merge` pattern already used for species yields.
- Without any code path change needed in macros (no new macro command required for this to
  work) — reaction counts are always collected once chemistry runs, mirroring how species
  scoring is always-on.

## Architecture & Approach

**Counting hook:** `TimeStepAction::UserReactionAction` resolves each reacting pair's
`G4MolecularConfiguration*` (via `GetMolecule(track)->GetMolecularConfiguration()`), looks up
the stable `G4DNAMolecularReactionData*` via
`G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB)`, determines the
current time bin from `G4Scheduler::Instance()->GetGlobalTime()`, and forwards
`(reactionData, time)` into a new `ReactionCounter` member owned by `TimeStepAction`. A
null/not-found reaction data pointer is logged via `DnaLogger` (warning) and skipped rather
than treated as fatal — this hook fires only for reactions the table itself resolved, so it
should not happen, but the code must not assume Geant4 internals guarantee it.

**New class `ReactionCounter.cc/.hh`** (parallel in spirit to `ScoreSpecies`): owns
`std::map<G4double /*bin*/, std::map<const G4DNAMolecularReactionData*, G4int>>`, using the
same fixed bin edges already used elsewhere (1,10,100 ps; 1,10,100 ns; ~1 µs) so its time axis
lines up with `Species.Txt`. Exposes `Record(reactionData, time)`,
`AbsorbResultsFromWorkerScorer(ReactionCounter*)` (merge, additive), `ASCII()` (writes
`Reactions.Txt`), and `WriteWithAnalysisManager()` (writes `Reactions_nt_reactions.csv` via
`G4AnalysisManager`, opened as its own file — `OutputDir::Resolve("Reactions")` — separate from
`Species` to avoid ntuple-ID collisions). Reaction-line formatting (`A + B -> C + D`) reuses
`ReactionTableDump.cc`'s existing formatting logic rather than duplicating it — exact
sharing mechanism (shared free function vs. small header) is an implementation-time call.

**MT merge + output wiring**, mirroring the existing `Run`/`ScoreSpecies` pattern exactly:
`Run`'s constructor additionally resolves the current thread's counter via
`dynamic_cast<TimeStepAction*>(G4Scheduler::Instance()->GetUserTimeStepAction())` (verified
accessor: `G4Scheduler::GetUserTimeStepAction()`,
`source/processes/electromagnetic/dna/management/include/G4Scheduler.hh:363`) and stores a
pointer to its `ReactionCounter`. `Run::Merge` sums worker counts into the master's counter
the same way it already merges `ScoreSpecies`. `RunAction::EndOfRunAction` (master only, after
the existing species-yield block) calls `ASCII()` then a merge/write+clear pass on the counter,
logging a status line matching the species block's shape.

**Time binning:** per-bin counts (histogram), not cumulative running totals — number of times
each reaction fired within each `[t_i-1, t_i]` interval, using the same edges as `ScoreSpecies`.

**Aggregation:** totals summed across all events in the run (one row per time bin/reaction),
not broken out per-event — matches the primary `Species_nt_species.csv` output; no `_all.csv`
per-event variant for this pass.

## Assumptions & Trade-offs

- Scope is bimolecular (track+track) reactions only, since `UserReactionAction` is the only
  mechanism that gives per-occurrence callbacks; this was explicitly confirmed with the user.
  Acid-base/scavenger reaction counts are NOT included and would need a separate, dedicated
  instrumentation mechanism (e.g. subclassing `G4DNAScavengerProcess` similar to the existing
  `ScavengerReactionAccess`) as explicit future work if ever wanted.
- Per-bin histogram counts (not cumulative totals) — chosen because reactions are discrete
  events, not a population with a natural "snapshot" reading, so a rate-over-time view is more
  meaningful than a running total.
- Aggregated-only output (no per-event breakdown) — simpler, matches the primary species
  output; per-event variance analysis is not a goal for this pass.
- Reaction counting is always-on once chemistry runs (no new macro command), unlike the static
  reaction-table dump which is opt-in via `/chem/reaction/dump` — counting has negligible
  overhead (one map increment per reaction) and there's no reason to gate it.
- This is bounded work: it fills an already-existing empty hook (`UserReactionAction`) and
  follows an already-established output/merge pattern (`ScoreSpecies`/`Run`/`RunAction`) — no
  new subsystem or architectural shift, no macro/messenger changes required.

## Open Questions

None outstanding. The two design choices that most needed verification going in — whether
`UserReactionAction` fires for the acid-base/scavenger network too (it does not, confirmed by
reading `G4ITModelProcessor.cc` and `G4DNAScavengerProcess.cc`), and whether reactant pairs can
have multiple product channels requiring a richer key than reactant pair alone (they cannot, in
this project's `PureWaterReactions.cc`) — were both resolved by reading Geant4 11.4.1 source
and the project's own reaction-table builder before presenting the design. Exact mechanism for
sharing reaction-line formatting with `ReactionTableDump.cc` (shared free function vs. small
header) is left as an implementation-time judgment call, not a design gap.

## Notes

- Prior art: `src/ReactionTableDump.cc`/`.hh` (static reaction-table dump, same-session
  predecessor feature) for reaction-line formatting conventions and the
  `G4DNAMolecularReactionTable::GetReactionData`/`GetVectorOfReactionData` API surface;
  `src/ScoreSpecies.cc` + `src/Run.cc` + `src/RunAction.cc` for the full time-binned
  scorer/merge/output pattern this feature reuses end-to-end.
- Verified in Geant4 11.4.1 source (not guessed, per project convention): the
  `UserReactionAction` call site
  (`source/processes/electromagnetic/dna/management/src/G4ITModelProcessor.cc:221`), the
  scavenger process's independent `PostStepDoIt` path
  (`source/processes/electromagnetic/dna/processes/src/G4DNAScavengerProcess.cc`), and the
  `G4Scheduler::GetUserTimeStepAction()` accessor
  (`source/processes/electromagnetic/dna/management/include/G4Scheduler.hh:363`).

## Token Usage

- **Input:** unavailable
- **Output:** unavailable
- **Cache read:** unavailable
- **Cache creation:** unavailable
- **Total:** unavailable
