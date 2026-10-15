# Session: source-layout-subdirs

**Date:** 2026-09-28T13:56:25.038Z
**Status:** Grill phase complete

## Problem Statement

`src/` and `header/` are two flat directories holding 31 `.cc` and 33 `.hh` files (plus `sim.cc` at the root). Finding everything about reaction counting, Chemistry selection or run actions means grepping, and new Chemistries/counters have no obvious home. CMake already globs recursively, but it also puts every header directory on the include path, so any file can include any header by bare name and nothing makes a directory boundary visible in the code.

Seeded from the scouted idea `source-layout-subdirs` (architecture review 2026-09-28). The scouted idea `rooted-includes` is folded into this session (see Q6 below); its entry will linger in `/gps status` afterwards and should be treated as done.

## Context & Constraints

- CMake: `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` over `src/*.cc` and `header/*.hh`; `project_include_dirs` is built by a foreach loop adding every header directory (transitional, to be removed). 7 unit-test targets list their `src/*.cc` dependencies by hand.
- Files that name paths: `CMakeLists.txt` (test targets), `CLAUDE.md` (~25 path refs), `README.md`, the `src/DnaChemistryList.cc` comment in 5 macros, `.claude/.claude-project.json` (generic wiring template only), `.claude/geant4-instructions.md` (to be grepped), `compile_commands.json` (tracked; regenerate, do not hand-edit).
- Frozen, not updated (historical records): `.kb-notes/*`, `.claude/plan-class-based-chemistry.md`, `.claude/plans/2026-09-28-named-chemistries.md`.
- Portable-file convention (`PureWaterReactions`, `PhysicsInteractionCounter`, ...): rooted includes mean the include prefix must be adjusted on copy; convention is reworded accordingly (Q7).
- `PrimaryKiller` is a `G4VPrimitiveScorer`; `TimeStepAction` is a `G4UserTimeStepAction` feeding `ReactionCounter`; `ReactionTableDump` is used by `DnaChemistryList` and `TimeStepAction`; `ChemUtils` is used only by `StackingAction`.
- Build dirs: `build/` is RelWithDebInfo (run `sim`), `build-ninja/` is Debug (ctest). Procedure in `.claude/geant4-instructions.md`.
- Project scope rules unchanged: single homogeneous water box, no UHDR-specific logic, no voxelization.

## Success Metrics

- Clean full rebuild succeeds from both `build/` and `build-ninja/`; all 7 CTest targets pass.
- `beam.in` and `beam_boscolo.in` (10 keV, `/run/beamOn 2`, `--dir`) produce byte-identical output files versus a baseline captured before any move. Any difference is stop-and-investigate.
- After the rewrite commit, a leftover bare project include is a build error (the multi-directory include loop is gone; `header/` is the only include root).
- `git log --follow` works on moved files (moves isolated in their own commit).
- No content change other than project `#include "..."` path prefixes, CMake path updates and docs.

## Architecture & Approach

Mirrored trees under `src/` and `header/` (header placement decided: keep `header/` mirrored, do not co-locate):

| Directory | Files |
|---|---|
| `core/` (5) | ArgParser, OutputDir, OutputDirMessenger, DnaLogger, DnaLoggerMessenger |
| `actions/` (8) | ActionInitialization, PrimaryGeneratorAction, RunAction, Run, EventAction, TrackingAction, StackingAction, SteppingAction |
| `geometry/` (2) | DetectorConstruction, DnaChemistryWorld |
| `physics/` (1) | PhysicsList |
| `chemistry/` (9) | DnaChemistryList, TimeStepAction, ChemUtils, ScavengerReactionAccess (header-only), ReactionTableDump, ChemistryTypes (header-only), ChemistryRegistry, ChemistrySelectMessenger, BuiltInChemistries |
| `chemistry/catalog/` (2) | PureWaterReactions, BoscoloChemReactions |
| `scoring/` (6) | ScoreSpecies, PrimaryKiller, ReactionCounter, PhysicsInteractionCounter, RunAccumulator, RunAccumulatorMessenger |

`sim.cc` stays at the root; `test/` stays flat (only its `src/` paths in CMake change).

Includes: project includes become rooted at `header/`, e.g. `"chemistry/DnaChemistryList.hh"`, in `src/`, `header/`, `test/` and `sim.cc`. A script substitutes only lines whose target exists under `header/`; G4/system includes untouched; no reordering. CMake drops the foreach include loop; `header/` is the sole include root for `sim` and the 7 test targets.

Commit structure (3 commits, branch created at plan write):
1. `git mv` moves plus CMake `src/` path updates for tests (builds green, since the old include loop still works).
2. Include rewrite plus dropping the include loop.
3. Docs and path sweep: `CLAUDE.md` (new short "Source layout" section, reworded portability convention), `README.md`, 5 macro comments, `.claude/` refs, regenerated `compile_commands.json`, new `docs/adr/0003-clustered-source-layout.md` (mirrored vs co-located headers; rooted vs bare includes). `CONTEXT.md` unchanged (directory names are not domain terms).

Verification protocol: baseline first (before any `git mv`), then clean rebuild + 7 CTest (`build-ninja/`) + rerun both macros + diff after commit 1 and commit 2.

## Assumptions & Trade-offs

- Moves only, no renames (Q2): renames (`ChemUtils`, `RunAccumulatorMessenger`, `BuiltInChemistries`, `CHEM6_PrimaryKiller_h` guard) belong to the scouted `file-renames` idea, done afterwards.
- Rooted includes cost some copy-paste portability of the portable files (prefix edit on copy) in exchange for visible directory boundaries and a self-verifying rewrite (accepted, Q7).
- `physics/` starts with a single file (`PhysicsList`); kept as a home for future physics options.
- No layering enforcement (Q10): cross-directory edges (e.g. `TimeStepAction` in `chemistry/` including `ReactionCounter` in `scoring/`) are only recorded as a baseline; enforcement belongs to the scouted `decouple-cross-cluster-includes` idea.
- Byte-identical output relies on the deterministic default RNG and no logic change.
- Commits 1 and 2 are separate so rename detection stays reliable; per-directory commits rejected (six builds for no extra safety).

## Open Questions

None blocking. Plan-time details: exact regex/script for the include rewrite; whether `.claude/geant4-instructions.md` and `.claude/hooks` contain stale path refs (grep at plan time); which build directory regenerates `compile_commands.json`.

## Notes

Decisions map to grill questions: Q1 mirrored headers; Q2 moves only; Q3 TimeStepAction to chemistry, ReactionCounter to scoring, DnaChemistryWorld to geometry; Q4 only `catalog/` under chemistry; Q5 flat `test/`; Q6 rooted includes now (absorbs `rooted-includes`); Q7 portability reworded; Q8 drop include loop; Q9 scope boundary; Q10 no enforcement; Q11 three commits; Q12 docs sweep + ADR 0003; Q13 baseline-diff verification.

## Token Usage

- **Input:** 28
- **Output:** 17691
- **Cache read:** 919441
- **Cache creation:** 22298
- **Total:** 959458
