# Session: geant4-dna-recipes-docs

**Date:** 2026-10-11T12:25:09.517Z
**Status:** Grill phase complete

## Problem Statement

dnachem-min has accumulated several reusable Geant4/Geant4-DNA implementation patterns (scorers, species counters, the mesoscopic 3D species model, HDF5 export, custom macro commands, selectable Chemistries) but the know-how for each is scattered across source files and ADRs. There is no single place that captures "how to do X" as a self-contained recipe. These recipes are meant to seed future Claude skills, so they need to exist as structured, verified documentation before any skill can be built from them.

## Context & Constraints

- **Current behavior:** The patterns exist only as working code (e.g. `src/scoring/ScoreSpecies` equivalent, `MesoSpeciesCounter.cc`, `MesoSpatialFile.cc`, `ChemistryRegistry.cc`, `*Messenger.cc` classes) plus scattered ADRs (`docs/adr/0002`, `0003`, `0004`, `0005`, `0006`, `0007`, `0008`) and skill docs (`.claude/skills/sim-output/SKILL.md`, `docs/output/README.md`). No single "how to build this kind of thing" document exists.
- **Pain point:** Building a new skill (or a new project) that needs e.g. a scorer, a species counter, or HDF5 export currently requires re-reading multiple source files and ADRs from scratch, and re-verifying Geant4/Geant4-DNA API behavior against source each time.
- **Dependencies:** Must follow the global ground rule of never guessing Geant4-DNA API behavior — every recipe's API claims must be verified against `geant4SourcePath` source (not just headers) or the KB at `${DEV_DIR}/GEANT4/geant4-v11.4.1-kb`, per `~/dotfiles/claude/COMMON_CLAUDE.md`. Recipes must not duplicate or contradict existing ADRs/skills; they should cross-reference them (e.g. point at `docs/adr/0006`, `.claude/skills/sim-output/SKILL.md`, `docs/output/SpeciesMesoSpatial-h5.md`) rather than restate their content.
- **Tech stack:** C++20, Geant4 11.4.1 with Geant4-DNA, HDF5 C++ bindings, CMake. Reference implementations live under `src/scoring/`, `src/chemistry/`, `src/core/`.

## Success Metrics

- `docs/recipes/` exists under the repo root with one markdown file per recipe topic plus an index (`README.md`) listing them.
- Each recipe documents the generic Geant4/Geant4-DNA pattern (verified against source/KB, not guessed) and cites this project's own classes (file:line) as a working reference implementation.
- The macro-command-list recipe is a living reference enumerating dnachem-min's actual custom commands (grouped by directory: `/chem/*`, `/run/*`, `/dnaLogger/*`, `/primaryKiller/*`, etc.), not a generic how-to.
- All 9 recipes exist: (1) create a scorer + export (Geant4), (2) create a species counter + export (Geant4-DNA), (3) 3D species counting with the mesoscopic model + export, (4) export data as HDF5, (5) create a new macro command, (6) list of dnachem-min's custom macro commands, (7) add a new Chemistry, (8) add a new reaction/molecule to an existing Chemistry, (9) add a new per-run counter/accumulator.

## Architecture & Approach

Create `docs/recipes/` with agent-facing how-to files (concise, source-verified, `file:line` pointers, no tutorial prose), one per topic, plus `docs/recipes/README.md` as an index. Each recipe: states the generic Geant4/Geant4-DNA mechanism first (verified against source/KB), then points at dnachem-min's own implementation as the concrete, working example, then cross-references the relevant ADR(s) or skill doc(s) instead of repeating their content. Style follows the existing `docs/output/README.md` convention (factual tables, concrete examples from real runs/code, links instead of duplication).

## Assumptions & Trade-offs

- Assuming "receipes"/"./doc/" in the original request were typos for "recipes"/"./docs/" — confirmed with the user.
- Scope is generic/portable Geant4-DNA knowledge with dnachem-min as the verified reference, not a dnachem-min-only walkthrough — so these recipes should remain useful if copied into another Geant4-DNA project.
- Not doing: no new code, no new skill files yet (this session only produces the recipe docs that a future skill-authoring session will draw on); no attempt to cover every Geant4-DNA pattern that exists anywhere in the source tree, only the 9 topics agreed here.
- The macro-command list is a living reference (will drift as commands are added); it documents current state, not a mechanism to auto-generate itself.

## Open Questions

None — design approved in this session.

## Notes

Originally requested topics (6): scorer+export, species counter+export, mesoscopic 3D species count+export, HDF5 export, new macro command, list of custom macro commands.
Added during the grill (3, because the project already has clean, reusable conventions for them): add a new Chemistry, add a new reaction/molecule, add a new per-run counter/accumulator.
