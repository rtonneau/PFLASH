# Session: dump-subfolder-per-run

**Date:** 2026-09-24T15:36:16.905Z
**Status:** Grill phase complete

## Problem Statement

`/run/dumpDataAndReset [prefix]` separates successive dumps only by a literal filename prefix (`run01_`, `run02_`). The user wants to dump each run into its own subfolder (`run01/`, `run02/`) with unprefixed filenames instead.

## Context & Constraints

- `OutputDir::Resolve` currently builds `<--dir>/<prefix><filename>`; nothing creates directories at dump time.
- `RunAccumulator::TryReservePrefix` makes reuse of a prefix in the same process fatal; files left on disk by earlier processes are silently overwritten.
- `G4AnalysisManager::OpenFile(OutputDir::Resolve("Species"))` already works with a `--dir` path; must be confirmed with a nested path.
- `OutputDir` and `RunAccumulator` logic stays pure and Geant4-free (portable-class convention, covered by `test/OutputDirTest.cc` and `test/RunAccumulatorTest.cc`).
- The success line `[RunAccumulatorMessenger] dumped and reset (prefix='...')` is a run.successMarkers check and must keep matching.
- The MT per-event `output_event_t*_e*.txt` files are written continuously outside dumps and stay in the top output directory.

## Success Metrics

- Macro `/run/beamOn 2`, `/run/dumpDataAndResetToDir run01`, `/run/beamOn 2`, `/run/dumpDataAndResetToDir run02` produces `run01/` and `run02/` under the output dir, each holding the full file set (Species.Txt, Species_nt_species*.csv, Reactions.Txt, Reactions_nt_reactions.csv, ReactionsMetadata.csv, EnergyDeposit.Txt, PhysicsInteractions.Txt/.csv).
- Reusing a name in the same process is fatal; absolute paths and `..` are rejected before anything is written.
- Unit tests pass in `build-ninja/` (ctest); existing `/run/dumpDataAndReset [prefix]` and the `EndOfRun_` safety net behave as before.

## Architecture & Approach

New command `/run/dumpDataAndResetToDir <name>` (Idle state, required parameter); the existing prefix command is untouched. Files:
- `src/OutputDir.cc`, `header/OutputDir.hh`: `ConfigureSubdir(name, err)` validates (relative, no `..`), creates folders (`create_directories`) and sets `gSubdir`; `ConfigureSubdir("")` clears it. `Resolve()` becomes `<dir>/<subdir>/<prefix><filename>`.
- `src/RunAccumulator.cc`, `header/RunAccumulator.hh`: `TryReserveSubdir(name, enforce, err)` with its own reserved set, alongside `TryReservePrefix`.
- `src/RunAccumulatorMessenger.cc`, `header/RunAccumulatorMessenger.hh`: second command; `DumpAndReset` gains a subfolder argument, sets it before writing and clears it after. Fatal `G4Exception` codes `InvalidDumpSubdir` and `DuplicateDumpSubdir`. Success line keeps `dumped and reset (prefix='...')` and gains `subdir='...'`.
- Tests in `test/OutputDirTest.cc` and `test/RunAccumulatorTest.cc`; short docs addition in `CLAUDE.md`. `macro/reactions.in` left alone.

## Assumptions & Trade-offs

- Explicit new command chosen over a flag on the existing command or an implicit trailing-`/` prefix: no parameter parsing, no accidental triggering.
- Reuse policy mirrors prefixes: same-process reuse is fatal, an existing folder on disk is reused and its files overwritten.
- Nested names (`scan1/run01`) allowed for parameter scans; absolute paths and `..` rejected so output cannot escape the output directory.
- Folder and prefix cannot be combined in one dump.

## Open Questions

None. Whether to switch `macro/reactions.in` to the new command was left out of scope unless the user asks.

## Notes

Session branch is created from the current HEAD, which is `feat/macro-dump-and-reset` (unmerged, with uncommitted changes in `.claude/`, `.gitignore` and `compile_commands.json`).

## Token Usage

- **Input:** 20
- **Output:** 7126
- **Cache read:** 642641
- **Cache creation:** 18114
- **Total:** 667901
