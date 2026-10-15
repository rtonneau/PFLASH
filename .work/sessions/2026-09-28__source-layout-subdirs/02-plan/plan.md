# Implementation Plan

**Session:** source-layout-subdirs
**Date:** 2026-09-28T14:10:36.004Z
**Estimated effort:** 0.5 day (mostly clean-rebuild and verification waits)

## Strategy

Cluster `src/` and `header/` into mirrored subdirectories (`core/`, `actions/`, `geometry/`, `physics/`, `chemistry/` with `chemistry/catalog/`, `scoring/`), then root every project include at `header/` and make `header/` the only project include directory. `sim.cc` stays at the root and `test/` stays flat. Spec: `.work/sessions/2026-09-28__source-layout-subdirs/01-grill/resume.md`.

Scope is fixed: `git mv` moves, project `#include` path prefixes, CMake path updates and docs. No renames, no logic edits, no include reordering, no header-guard changes, no G4/system include changes. The pass condition is byte-identical `sim.exe` output against a baseline captured before any move.

Mapping used by every ticket (headers mirror sources):

- `core/`: ArgParser, OutputDir, OutputDirMessenger, DnaLogger, DnaLoggerMessenger
- `actions/`: ActionInitialization, PrimaryGeneratorAction, RunAction, Run, EventAction, TrackingAction, StackingAction, SteppingAction
- `geometry/`: DetectorConstruction, DnaChemistryWorld
- `physics/`: PhysicsList
- `chemistry/`: DnaChemistryList, TimeStepAction, ChemUtils, ScavengerReactionAccess, ReactionTableDump, ChemistryTypes, ChemistryRegistry, ChemistrySelectMessenger, BuiltInChemistries
- `chemistry/catalog/`: PureWaterReactions, BoscoloChemReactions
- `scoring/`: ScoreSpecies, PrimaryKiller, ReactionCounter, PhysicsInteractionCounter, RunAccumulator, RunAccumulatorMessenger

Conventions for all tickets:

- Scratch dir `S=.scratch/tests/2026-09-28__source-layout-subdirs` (git-ignored) holds the baseline, scratch macros, scripts and logs.
- Builds run through the PowerShell tool with the MSVC environment, in the background, logging to a file:
  ```powershell
  $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build <dir> --config <type> --clean-first --target <targets>"
  ```
  `build/` is RelWithDebInfo (target `sim`), `build-ninja/` is Debug (the 7 test targets: ArgParserTest, OutputDirTest, ReactionTableDumpTest, ReactionCounterTest, PhysicsInteractionCounterTest, RunAccumulatorTest, ChemistryRegistryTest). `--clean-first` gives the clean full rebuild; `CONFIGURE_DEPENDS` re-globs by itself.
- Each ticket ends with one commit. End every commit message with:
  ```
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01LzPFxun3ELJPD5nfPa6LG7
  ```
- Working-tree files are CRLF (the index is LF). Nothing in this plan should change line endings.
- The hook log `.claude/hooks/hook-posttooluse.log` and `.work/` state files show as modified; never stage them.

After `/gps write`, copy this plan to `.claude/plans/2026-09-28-source-layout-subdirs.md`, as the previous session did (project rule: plans live in `.claude/plans/`).

## Tickets Overview

1. `01-move-into-clusters`: capture the baseline (twice, to prove determinism), `git mv` all 64 files into the clusters, update the 7 test targets' `src/` paths. Bare includes still compile because the old include loop remains. Commit 1.
2. `02-rooted-includes`: rewrite every project include to its `header/`-rooted path with a script, replace the include loop with a single `header/` root. Commit 2.
3. `03-docs-adr-compile-commands`: update `CLAUDE.md`, `README.md`, macro comments and `.claude/.claude-project.json`, add ADR 0003, regenerate `compile_commands.json`, update the portable-class memory note. Commit 3.

## Sequencing Rationale

Moves come first and alone so `git log --follow` and rename detection stay reliable (all 64 renames at 100% similarity). They build on their own because the per-directory include loop is still in CMake. The include rewrite comes second: it changes only include lines, and removing the loop makes any missed include a build error. Docs come last because they describe the final layout and need the moved tree to compute new paths. Each code ticket re-runs the same clean rebuild, the 7 CTest targets and the output diff, so a regression points at exactly one commit.

## Risks & Mitigation

- **Output is not deterministic run to run:** the baseline is captured twice and diffed before any move. If the two differ, stop and report which files; do not invent an exclusion list.
- **Rewrite script misses an include** (unusual spacing, a header outside `header/`): dropping the include loop turns it into a compile error; fix that line by hand and say so in the commit body.
- **Rewrite script touches more than include lines:** a `git diff -U0` filter must show only `#include` lines changed.
- **CRLF damaged:** the script substitutes inside a line only and writes with the same encoding; check `git diff --stat` shows no whole-file changes.
- **Long builds hit the tool timeout:** run builds and simulations with `run_in_background` and read the log.
- **Stale `compile_commands.json` entries:** regenerate through the hook and grep for leftover top-level `src/<Name>.cc` paths.

## Assumptions

- The default RNG is deterministic, so output is byte-identical between runs of the same binary.
- `beam.in` (100 keV) is too slow to use; two scratch macros at 10 keV with `/run/beamOn 2` (pure water and `BoscoloChem`) stand in for it.
- Geant4/system headers never share a basename with a project header, so rewriting by basename is safe.
- `.claude/geant4-instructions.md` has no stale paths; the only `.claude/` path reference to fix is the test-wiring line in `.claude-project.json`.
- Historical notes (`.kb-notes/*`, `.claude/plan-class-based-chemistry.md`, older `.claude/plans/*`) stay untouched.

## Token Usage

- **Input:** 28
- **Output:** 33609
- **Cache read:** 1469453
- **Cache creation:** 45608
- **Total:** 1548698
