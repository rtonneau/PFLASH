# Session: --dir argument should also have a macro dile entry associated. But if --dir arg is passed and the related entry in macro file is also present then the app should warn and interrupt

**Date:** 2026-09-23T13:02:52.400Z
**Status:** Grill phase complete

## Problem Statement

The `--dir` CLI flag redirects all output files but has no macro-file equivalent — there's no way to set the output directory from a macro alone. Add a macro command counterpart, and detect the case where both `--dir` and the macro command are used with conflicting values, warning and interrupting instead of silently letting one win.

## Context & Constraints

- `OutputDir::Configure(dir, err)` (`src/OutputDir.cc`) is a pure, process-wide, testable function: sets a single global `gConfiguredDir`, creating the directory if needed (non-recursive — parent must exist).
- `main()` in `sim.cc:77` calls `OutputDir::Configure()` from the `--dir` CLI flag *before* the macro file is executed via `/control/execute` (`sim.cc:143`) — so by the time any macro command runs, the CLI-configured state is already final.
- Existing precedent for "two config sources for one setting conflict → fatal": `DnaChemistryList::ApplyReactionTimeBinning()` (`src/DnaChemistryList.cc:135-143`), which raises `G4Exception(..., FatalException, ...)` when both `/chem/reaction/timeBinsFixed` and `timeBinsList` are issued.
- No existing messenger in this project has a unit test (messengers need G4 runtime); only pure logic (e.g. `OutputDir::Configure`) is covered by `test/OutputDirTest.cc`, which needs no Geant4 runtime init.
- `CMakeLists.txt` globs `src/*.cc` and `header/*.hh` (`CONFIGURE_DEPENDS`), so new source files are picked up automatically — no CMakeLists changes needed for the new messenger.

## Success Metrics

- A macro can set the output directory via `/run/outputDir <path>` with no `--dir` flag needed, and it behaves like `--dir` (directory created if missing, all output files redirected there).
- Running with `--dir <pathA>` and a macro containing `/run/outputDir <pathB>` (different path) aborts the run with a clear fatal error instead of silently picking one.
- Reissuing the same path (via `--dir` then macro, or macro then macro) is a harmless no-op, not an error.
- All new pure logic is covered by unit tests in `test/OutputDirTest.cc`, runnable without Geant4 runtime init.

## Architecture & Approach

1. **`OutputDir.cc`/`OutputDir.hh`**: add `OutputDir::ConfigureFromMacro(const G4String &dir, G4String &err)`.
   - If no directory is configured yet (`--dir` absent/empty), behaves exactly like `Configure()`: creates the dir if needed, sets it.
   - If a directory is already configured and `dir` is the same raw string, no-op, returns true.
   - If a directory is already configured and `dir` differs, returns false with an error describing the conflict — works symmetrically regardless of whether the existing value came from `--dir` or an earlier macro call.
   - Path comparison is a plain string comparison, not canonicalized (e.g. `./out` vs `out` would be flagged as "different" even though they resolve to the same directory) — accepted as a known limitation, not solved by path canonicalization.
2. **New `header/OutputDirMessenger.hh` + `src/OutputDirMessenger.cc`**: a small `G4UImessenger` (mirrors `DnaLoggerMessenger`'s structure) wrapping one `G4UIcmdWithAString("/run/outputDir", ...)`, restricted to `PreInit` state (same restriction pattern as `/chem/env/pH`). `SetNewValue` calls `OutputDir::ConfigureFromMacro`; on failure raises `G4Exception("OutputDirMessenger::SetNewValue", "ConflictingOutputDir", FatalException, err)` — same pattern as `ConflictingTimeBins`, which both warns and aborts.
3. **`sim.cc`**: instantiate `OutputDirMessenger` alongside `dnaLoggerMessenger` (created early, before the macro executes), and `delete` it during cleanup alongside `dnaLoggerMessenger`.

## Assumptions & Trade-offs

- `/run/outputDir` was chosen (over a new `/output/` top-level directory) to nest under the existing `/run/` G4 command tree.
- Conflict detection triggers only when the CLI and macro paths differ as raw strings — not when both are present but equal. Equal values are treated as redundant-but-harmless.
- Macro-vs-macro conflicts (two differing `/run/outputDir` calls, no `--dir` involved) are handled by the same mechanism and same fatal-conflict behavior, since there's only one `gConfiguredDir` slot — this wasn't explicitly requested but falls out naturally from the design and is consistent with the "conflict = fatal" theme.
- `PreInit`-only restriction enforces that the output directory is finalized before `/run/initialize` spawns worker threads (MT mode), matching `OutputDir.hh`'s existing documented thread-safety requirement.

## Open Questions

None outstanding — design was presented in chat and approved as-is.

## Notes

Bounded-path feature (existing flow being extended, not a new subsystem). No `02-plan/plan.md` or tickets — implementation proceeds directly via the normal dev workflow (TDD) per the brainstorming skill's bounded path.

## Token Usage

- **Input:** 40
- **Output:** 15743
- **Cache read:** 1453947
- **Cache creation:** 33298
- **Total:** 1503028
