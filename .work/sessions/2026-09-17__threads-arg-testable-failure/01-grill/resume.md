# Session: threads-arg-testable-failure

**Date:** 2026-09-17
**Status:** Grill phase complete (bounded)

## Problem Statement

`ParseThreadsArg` in `sim.cc` (added in the prior `multithreading-cli-option` session) validates the `--threads N` CLI flag, but its only failure mode is `exit(1)` called from inside the function itself. Its validation rules (missing value, non-numeric value, value `<= 0`, value exceeding available cores) can't be exercised by a test without killing the test process.

## Context & Constraints

- **Current behavior:** `sim.cc:32-64` defines `static G4int ParseThreadsArg(int argc, char **argv)`, which scans `argv` for `--threads N`. On any validation failure it calls a local `fail` lambda that sets `DnaLogger` to `Error`, logs `"--threads: <message>"`, and calls `exit(1)`. On success it returns the parsed thread count (`> 0`), or `0` if the flag wasn't present. `main()` (`sim.cc:89`) uses the return value to pick `Serial` vs. `MT` `G4RunManagerType`.
- **Source:** flagged by the architecture-scout report (`scout-reports/architecture-review-2026-09-17T15-51-54-188Z.html`), strength "Strong", seed `threads-arg-testable-failure`.
- **No test infrastructure exists yet.** The earlier `geant4-testing-infrastructure` initiative (spec + 7 CTest tickets under `.scratch/geant4-testing/`) was only ever planned, never implemented, and that `.scratch` directory has since been deleted. There is no `enable_testing()`/CTest/GoogleTest wiring anywhere in `CMakeLists.txt` today. This session adds the first seed of that, scoped narrowly to this one function — not a revival of the broader abandoned initiative.
- **Dependencies:** Must preserve existing `--threads` CLI behavior exactly (same validation rules, same order, same error messages, same exit code) — this is a refactor of the failure mechanism, not a behavior change. Must not disturb the existing `argv[1]` = macro-file convention.
- **Tech stack:** Geant4 11.4 (`G4Threading::G4GetNumberOfCores()`, `G4String`), C++20, CMake 3.16+, MSVC x64 toolchain (per `CLAUDE.md` build instructions). No test framework dependency introduced — plain assert-based test code only.

## Success Metrics

- `ParseThreadsArg` (or its renamed equivalent) is a pure function: given `argc`/`argv`, it returns a result without calling `DnaLogger` or `exit()`.
- A new standalone test executable exercises all four failure cases and both success cases (flag absent, valid `--threads N`) without needing a Geant4 runtime or `DnaLogger` initialization.
- `ctest` (or running the test binary directly) reports all cases passing.
- `sim`'s existing `--threads` behavior is unchanged when manually re-verified: default Serial, `--threads 2` → MT with 2 workers, bad values → error message + exit 1.

## Architecture & Approach

- Extract the parsing logic out of `sim.cc` into `header/ThreadsArg.hh` + `src/ThreadsArg.cc`, following this project's existing modular layout (e.g. `DnaLogger.cc`/`.hh`).
- New signature, returning a plain result struct instead of exiting:
  ```cpp
  struct ThreadsArgResult {
    bool ok;
    G4int count;      // validated thread count, or 0 if --threads wasn't given
    G4String error;   // human-readable message, only meaningful when ok == false
  };
  ThreadsArgResult ParseThreadsArg(int argc, char **argv);
  ```
- The function is fully free of side effects: no `DnaLogger` calls, no `exit()`. Same validation order/rules as today.
- `sim.cc`'s `main()` becomes the thin adapter: calls `ParseThreadsArg`, and on `!ok` reproduces what the old `fail` lambda did (`DnaLogger::SetLevel(Error)`, `DnaLogger::Print(Error, "--threads: " + result.error)`, `exit(1)`).
- New standalone test `test/ThreadsArgTest.cc`: plain `assert`-based checks (no framework) covering the four failure cases plus the two success cases.
- `CMakeLists.txt`: add `enable_testing()`, a new `ThreadsArgTest` executable target (`test/ThreadsArgTest.cc` + `src/ThreadsArg.cc`, same include dirs; needs only `G4Threading`/`G4String`, no Geant4 runtime init), and `add_test(NAME ThreadsArgTest COMMAND ThreadsArgTest)`.

**Files touched:** `sim.cc`, new `header/ThreadsArg.hh`, new `src/ThreadsArg.cc`, new `test/ThreadsArgTest.cc`, `CMakeLists.txt`.

## Assumptions & Trade-offs

- Chose a plain result struct (`ThreadsArgResult`) over `std::optional<G4int>` + error out-param — C++20 doesn't have `std::expected` (C++23), and a single struct keeps success/failure data together in one return value.
- Chose a minimal standalone test (plain asserts, one new executable) over either "no test at all" or "stand up a full framework (GoogleTest)" — no test infra exists yet in this repo, and a full framework is out of scope for a single-function refactor. This is a deliberately narrow first step, not a revival of the abandoned 7-ticket CTest initiative.
- Chose to also add `enable_testing()` + a single `add_test(...)` (real `ctest` entry point) rather than a target that's only run by hand — cheap to add now, gives a real regression check with no added dependency.
- Not touching the `runconfig-resolver` or `gui-batch-seam` candidates from the same scout batch — separate seeds, out of scope here.
- This is a **bounded** change (existing flow being extended, not a new subsystem), so per the brainstorming skill's bounded path this session skips the formal plan/tickets flow and goes straight from an in-chat design approval to implementation.

## Open Questions

None outstanding — design fully specified and confirmed via clarifying questions (test scope, extraction location, return type shape, side-effect purity, CTest wiring).

## Notes

- Prior session (`multithreading-cli-option`) implemented the original `ParseThreadsArg` with the `exit(1)`-inside-the-function pattern being refactored here.
- Design context came from the `threads-arg-testable-failure` scout seed (`.work/sessions/scout-reports/architecture-review-2026-09-17T15-51-54-188Z.html`), consumed from `.pending-seeds.json` when this session started.
- Implementation not yet started as of this write.
