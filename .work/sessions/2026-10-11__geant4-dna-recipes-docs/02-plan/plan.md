# Implementation Plan

**Session:** geant4-dna-recipes-docs
**Date:** 2026-10-11T12:29:31.057Z
**Estimated effort:** 1 day

## Strategy

Ticket 1 scaffolds `docs/recipes/README.md` as the index, with the exact filenames every later ticket must produce — so link-checking is mechanical. Tickets 2-10 each write one recipe file, independently committable, in no particular dependency order (they can ship in any order after ticket 1, but are sequenced to match the originally requested list first, then the three added-during-grill topics). Every recipe follows the same fixed section skeleton (What this is / Generic pattern / dnachem-min reference implementation / Serial vs MT / See also) so the verification step is the same `grep` pattern across all nine, and so a future skill-authoring pass can parse them uniformly. The "Serial vs MT" section is a late addition requested mid-session: each recipe must state, verified against source (not guessed), how that topic's data behaves under `--threads N` vs the Serial default (sim.cc:118-120) — in particular whether the mechanism is per-thread-then-merged (the `Run::Merge` pattern, src/actions/Run.cc:84-111, used by scorers via `AbsorbResultsFromWorkerScorer`, `ReactionCounter`, `PhysicsInteractionCounter`, `MesoSpeciesCounter`, `TrackLengthTable`), guarded by an explicit mutex instead (`MesoSpatialFile::AppendEvent`, src/scoring/MesoSpatialFile.cc:22-25,139-144), or a PreInit macro command replayed identically to every worker thread by the MT run manager (verify this specific claim against the Geant4 source for `G4MTRunManager` before stating it, rather than asserting from general Geant4 knowledge).

## Tickets Overview

- **Ticket 1:** Scaffold `docs/recipes/` with `README.md` index linking all 9 planned recipe files by their final names.
- **Ticket 2:** Recipe: create a scorer and export data (Geant4) — generic `G4VPrimitiveScorer` mechanics, `RegisterPrimitive`, MT merge via `AbsorbResultsFromWorkerScorer`.
- **Ticket 3:** Recipe: create a species counter and export data (Geant4-DNA) — the DNA-specific layer on `ScoreSpecies` (chemistry-stage hook, export format), cross-referencing ticket 2 instead of repeating scorer basics.
- **Ticket 4:** Recipe: 3D species counting with the mesoscopic model and export — `MesoSpeciesCounter`, `G4DNAEventScheduler`, `TimeStepAction`'s hand-over.
- **Ticket 5:** Recipe: export data as HDF5 — `MesoSpatialFile`'s stage-then-move pattern and its mutex-guarded writer.
- **Ticket 6:** Recipe: create a new macro command — `G4UImessenger`/`G4GenericMessenger`, PreInit vs Idle commands, the `DeclareMethod` string-truncation pitfall.
- **Ticket 7:** Recipe: dnachem-min's custom macro command reference — living enumeration of every actual command, grouped by directory.
- **Ticket 8:** Recipe: add a new Chemistry (reaction set) — `ChemistryRegistry`/`BuiltInChemistries.cc` pattern.
- **Ticket 9:** Recipe: add a new reaction or molecule to an existing Chemistry — the plain-data builder pattern (e.g. `PureWaterReactions.cc`).
- **Ticket 10:** Recipe: add a new per-run counter/accumulator — the portable-counter convention fed from `RunAccumulator`.

## Sequencing Rationale

Ticket 1 must land first since it fixes every later filename. Tickets 2-10 touch disjoint files (one new `.md` each) and carry no interface dependency on each other, so they are independently committable and can ship in any order; the listed order simply matches the originally requested 6 topics before the 3 added during the grill.

## Risks & Mitigation

- **Risk:** A recipe states a Geant4/Geant4-DNA API behavior from memory instead of verified source, repeating the exact mistake these recipes exist to prevent. → **Mitigation:** Acceptance criteria require a `file:line` citation for every non-trivial claim, checked against `geant4SourcePath` or the KB, per each ticket's Notes.
- **Risk:** The new "Serial vs MT" section invents MT behavior for a class nobody checked (e.g. a messenger's PreInit replay). → **Mitigation:** Notes call out the one claim (PreInit command replay to workers) that is NOT yet verified in this session and must be checked against `G4MTRunManager` source before ticket 6 and ticket 7 assert it.
- **Risk:** Nine recipes drift into inconsistent structure, making them hard to turn into skills later. → **Mitigation:** fixed section skeleton, enforced by an identical grep-based verification step on every ticket.

## Assumptions

- "receipes"/"./doc/" in the original request were typos for "recipes"/"./docs/" (confirmed with the user).
- No new code or skill files in this session — only the recipe docs; a future session builds skills from them.
- The macro-command reference (ticket 7) documents current state as of this session; it is expected to drift as commands are added later and is not meant to regenerate itself.
