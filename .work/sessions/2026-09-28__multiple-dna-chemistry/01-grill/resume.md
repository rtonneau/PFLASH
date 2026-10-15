# Session: multiple-dna-chemistry

**Date:** 2026-09-28T12:18:57.171Z
**Status:** Grill phase complete

## Problem Statement

dnachem-min has a single, hard-coded chemistry (`PureWater`: pure-water + O2-derived reaction network from `PureWaterReactions.cc`, plus an always-on acid-base buffer network registered in `DnaChemistryList::RegisterAcidBaseScavengerProcesses`). To reproduce chemistries used by other libraries and papers (first target: `BoscoloChem`), the user must be able to define several named chemistries and pick one from the macro file before `/run/initialize`.

## Context & Constraints

- `PhysicsList`'s constructor builds one `DnaChemistryList` before any macro runs; the chemistry content is only built at `/run/initialize` (`ConstructMolecule`, `ConstructDissociationChannels`, `ConstructReactionTable`, `ConstructTimeStepModel`, `ConstructProcess`). A PreInit macro command can therefore select the chemistry with no `sim.cc` restructuring.
- `DnaChemistryList` already owns a `/chem/reaction/` messenger; `DnaChemistryWorld` owns `/chem/env/`. Use `G4GenericMessenger` (`DeclareProperty` for multi-token strings; see memory note on `DeclareMethod` truncation).
- Time-step model stays SBS only; molecules and dissociation channels stay shared (`G4ChemDissociationChannels_option1`); `DnaChemistryWorld` and `G4DNAScavengerMaterial` plumbing unchanged.
- ADR 0001 made the acid-base network unconditional; ADR 0002 (written during the grill) relaxes this: per-Chemistry, may be empty. 0001 still holds for `PureWater`.
- New logic classes should be copy-paste portable (no project-specific includes), like `PureWaterReactions.cc`.
- Project rules: DnaLogger for diagnostics, unit tests built/run from `build-ninja/` (Debug), sim smoke test from `build/` with 10 keV e- and `/run/beamOn 2`.
- Glossary: `CONTEXT.md` now defines **Chemistry** (named, selectable reaction-network variant; PascalCase names, case-insensitive; all share molecules/channels; default `PureWater`). Avoid "chemistry model" and "chemistry list" for this concept.

## Success Metrics

- `PureWater` regression: `/chem/reaction/dump` output and a same-seed `beam.in` run (species yields) are identical before and after the change.
- Kernel-free unit tests in `build-ninja/` for the registry: name lookup, case-insensitivity, unknown-name error text listing valid names, default = `PureWater`, conflicting selection, same-name repeat is a no-op.
- Smoke run (10 keV e-, `/run/beamOn 2`) succeeds for each Chemistry, including one with an empty acid-base list (must confirm installing `G4DNAScavengerMaterial` with no scavenger processes is safe).
- `beam.in` and other existing macros work unchanged.
- `BoscoloChem` physics is NOT validated against the paper; the user owns its values.

## Architecture & Approach

1. **Chemistry = reaction content.** A named variant of the reaction table plus the acid-base buffer reaction list. Stock Geant4 lists (`G4EmDNAChemistry_option*`) are out of scope (they bypass `DnaChemistryWorld` and the bulk-scavenger machinery).
2. **Selection.** `/chem/select <name>`, PreInit only. Names PascalCase, matched case-insensitively. Default `PureWater`. Unknown name: fatal `G4Exception` listing valid names. Two different names in one macro: fatal (like `ConflictingOutputDir`); same name twice: no-op. No switching after initialization (one Chemistry per process). `/chem/list` prints available names.
3. **Structure.** One portable file per Chemistry (`PureWaterReactions.cc`, new `BoscoloChemReactions.cc`), each exposing a reaction-table builder and an acid-base list builder, with no project-specific includes. A small registry maps name to builder. `DnaChemistryList::ConstructReactionTable` and `RegisterAcidBaseScavengerProcesses` read from the selected entry. `PureWater`'s acid-base list is today's hard-coded list moved into its file. `BoscoloChem` starts as a copy of `PureWater`, clearly marked work-in-progress; the user edits reactions in place.
4. **Acid-base list.** Per-Chemistry; may be empty or partial (empty means no `G4DNAScavengerProcess` registered; bulk pseudo-species remain via the shared molecule set but are inert). `G4DNAScavengerMaterial` is still installed unconditionally. The author of a Chemistry owns its physical consistency and states in the file header when the buffer is omitted.
5. **Global commands unchanged.** `/chem/env/pH`, `/chem/env/O2`, `/chem/reaction/*`, `/process/chem/TimeStepModel` behave identically for every Chemistry; a Chemistry cannot set its own defaults for them.
6. **Traceability.** Selected Chemistry name in the DnaLogger Info line at startup; the registry exposes a name accessor. No output-format changes.
7. **Docs.** Update `CLAUDE.md` (Key Files, Macro sections), README, source-file headers; add `macro/beam_boscolo.in` issuing `/chem/select BoscoloChem`; `beam.in` unchanged.
8. **Already written during the grill:** `CONTEXT.md` (Chemistry term; Pure-water chemistry reworded), `docs/adr/0002-named-chemistries.md`, pointer added to ADR 0001.

## Assumptions & Trade-offs

- Assumed all target chemistries fit the shared molecule set and dissociation channels (option1). A paper needing extra species or different channels is a larger, separate change.
- Trade-off: relaxing ADR 0001 lets users reproduce buffer-less published networks, at the cost that a Chemistry can be physically inconsistent (proton/hydroxide sink without source); responsibility sits with the Chemistry author.
- Trade-off: wrapping stock Geant4 chemistry lists rejected to keep `DnaChemistryWorld`/scavenger-material coupling.
- Assumed the registry is read-only after static registration, so MT worker threads can read it safely (`ConstructReactionTable` runs on master; `ConstructProcess` may run per worker).
- `/chem/list` output assumed to be plain `G4cout` (explicit user command), not DnaLogger-gated. Confirm during implementation.

## Open Questions

- Is installing `G4DNAScavengerMaterial` with zero registered scavenger processes safe? Verify via the empty-list smoke run.
- Exact registry shape (static function table vs. small struct of builder functions) and whether it is kernel-free enough to unit-test in `build-ninja/` without linking Geant4; decide at planning.

## Notes

- Side quest, NOT in this session: a run-environment companion file (env.md/toml/yaml; simulation data contributed from various places in the code, dumped with `/run/dumpDataAndResetToDir`, update of its content managed somewhere). Start separately with `/gps start run-environment-record` after this feature; the registry's name accessor lets it pick up the Chemistry name later.
- Session was started with slug `multiple-dna-chemistry` (shortened from the original request text).

## Token Usage

- **Input:** 34
- **Output:** 17902
- **Cache read:** 1135290
- **Cache creation:** 42928
- **Total:** 1196154
