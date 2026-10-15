# Session: arg-parser-refactor

**Date:** 2026-09-18T08:05:02.119Z
**Status:** Grill phase complete (bounded)

## Problem Statement

`header/ThreadsArg.hh` + `src/ThreadsArg.cc` (added in the prior `threads-arg-testable-failure` session, currently uncommitted) validate only the `--threads N` CLI flag, one flag per translation unit. Adding any future CLI flag would mean creating a new `SomethingArg.hh`/`.cc` pair each time, each reimplementing its own argv-scanning loop. This session replaces that pattern with a single, general-purpose `ArgParser` so future flags register into the same translation unit instead of spawning new files.

## Context & Constraints

- **Current behavior:** `header/ThreadsArg.hh` declares `ThreadsArgResult ParseThreadsArg(int argc, char **argv)`; `src/ThreadsArg.cc` scans `argv` for `--threads N`, validating (in order) that a value follows, that it's a clean positive integer, and that it doesn't exceed `G4Threading::G4GetNumberOfCores()`. It returns a plain result struct (`ok`, `count`, `error`) with no side effects — no logging, no `exit()`. `sim.cc`'s `main()` (`sim.cc:52-58`) calls it and, on `!ok`, reproduces the old `fail` lambda behavior itself: `DnaLogger::SetLevel(Error)`, `DnaLogger::Print(Error, "--threads: " + threadsArg.error)`, `exit(1)`. `test/ThreadsArgTest.cc` covers this with plain `assert`-based checks, wired into CTest via `CMakeLists.txt` (`enable_testing()` + `add_test(NAME ThreadsArgTest ...)`).
- **Pain point:** The one-file-per-flag pattern doesn't scale — each new CLI flag would need its own header/source pair reimplementing argv scanning, with no shared mechanism for common concerns (missing value, type parsing, per-flag custom validation).
- **Dependencies:** Must preserve `--threads`' exact current behavior: same three validation rules in the same order, same error message text (`"--threads: missing value"`, `"--threads: value must be a positive integer, got '<value>'"`, `"--threads: requested <N> threads, but only <M> cores are available"`), same exit code path in `sim.cc`. Must not disturb the `argv[1]` = macro-file convention.
- **Tech stack:** Geant4 11.4 (`G4Threading::G4GetNumberOfCores()`, `G4String`, `G4int`, `G4bool`), C++20, CMake 3.16+, MSVC x64 toolchain. No test framework dependency — plain assert-based test code only, matching the existing `ThreadsArgTest.cc` style.

## Success Metrics

- A single `ArgParser` class (`header/ArgParser.hh` + `src/ArgParser.cc`) supports registering `Int`, `String`, and `Bool` flags, each with an optional custom validator callback, and exposes `Parse(argc, argv)` plus typed getters (`GetInt`, `GetString`, `GetBool`) by flag name.
- `sim.cc` registers `--threads` as an int flag with a validator enforcing the core-count rule; `main()`'s error-handling code is unchanged (still calls `DnaLogger::Print` + `exit(1)` on failure), just fed from `parser.GetError()` instead of `threadsArg.error`.
- `test/ArgParserTest.cc` replaces `test/ThreadsArgTest.cc`, covering both generic parser mechanics (missing value, malformed int, unknown flag, bool/string flags) and the `--threads` registration's core-count validator specifically.
- `ctest` reports all cases passing; `sim`'s existing `--threads` CLI behavior (default Serial, `--threads 2` → MT, bad values → identical error text + exit 1) is manually re-verified unchanged.

## Architecture & Approach

- `ArgParser` is a registration-based parser, Geant4-light (only depends on `globals.hh` for `G4String`/`G4int`/`G4bool` — no `DnaLogger`, no `exit()`, fully free of side effects, matching `ThreadsArg`'s original purity goal).
- Three registration methods: `AddIntFlag(name, validator = nullptr)`, `AddStringFlag(name, validator = nullptr)`, `AddBoolFlag(name)` (presence-only, consumes no value). `Int`/`String` validators have signature `bool(value, G4String& err)` — an optional extra check layered on top of `ArgParser`'s own built-in type/format checks (e.g. "is a clean positive integer" for `Int`), used for flag-specific domain rules like `--threads`' core-count limit.
- `Parse(argc, argv)` scans `argv` once against all registered flags, populates parsed values internally, and returns `bool`. On failure, `GetError()` returns one fully-formed message already prefixed with the flag name (e.g. `"--threads: missing value"`), so callers don't reassemble the prefix themselves.
- After a successful `Parse()`, typed getters (`GetInt`, `GetString`, `GetBool`) read values back by flag name, returning a documented default (e.g. `0` for an absent int flag) when that flag wasn't present on the command line.
- `sim.cc`'s `main()` builds an `ArgParser`, registers `--threads` (int, with the core-count validator closure capturing `G4Threading::G4GetNumberOfCores()`), calls `Parse`, and on failure runs the same `DnaLogger::SetLevel(Error)` / `DnaLogger::Print(Error, parser.GetError())` / `exit(1)` sequence as today. On success, `parser.GetInt("--threads")` replaces `threadsArg.count`.
- `CMakeLists.txt`: rename the `ThreadsArgTest` executable/target to `ArgParserTest`, sourced from `test/ArgParserTest.cc` + `src/ArgParser.cc`, same `add_test(NAME ArgParserTest COMMAND ArgParserTest)` wiring.

**Files touched:** new `header/ArgParser.hh`, new `src/ArgParser.cc`, new `test/ArgParserTest.cc`; delete `header/ThreadsArg.hh`, `src/ThreadsArg.cc`, `test/ThreadsArgTest.cc`; update `sim.cc` and `CMakeLists.txt`.

## Assumptions & Trade-offs

- Chose a registration-based parser (flags register with `AddXFlag`, values read back via typed getters) over a single hand-written struct-and-if/else parse function — the explicit goal of this refactor is that adding a future flag means one registration call in `sim.cc`, not a new file or a growing if/else chain in `ArgParser.cc` itself.
- Chose typed getters by name (`GetInt("--threads")`) over per-flag registration callbacks — flexible for flags of different types without changing `ArgParser`'s core, at the cost of a flag-name typo only failing at runtime rather than at compile time. Accepted given this project's small, hand-reviewed flag set.
- Chose to support `Int`, `String`, and `Bool` flag types now, even though `--threads` is still the only real flag today — deliberately ahead of strict YAGNI, per your explicit call, so the next flag (likely a string or bool) doesn't need a type added to `ArgParser` first.
- Kept the core-count validator as an optional callback on `AddIntFlag` rather than a separate post-`Parse()` check in `sim.cc` — preserves today's single unified error path and message set for `--threads` exactly, instead of splitting its validation across `ArgParser` and `main()`.
- Kept `ArgParser` Geant4-DNA-agnostic (no coupling to `DnaLogger` or the core-count check itself — that lives in the validator closure `sim.cc` supplies), matching this project's copy-paste-portable convention already used for `PureWaterReactions.cc`.
- Not touching the `runconfig-resolver` or `gui-batch-seam` scout candidates from the same batch that originally produced `threads-arg-testable-failure` — separate seeds, out of scope here.
- This is a **bounded** change (generalizing existing, still-uncommitted code, not introducing a new subsystem), so per the brainstorming skill's bounded path this session skips the formal plan/tickets flow and goes straight from an in-chat design approval to implementation.

## Open Questions

None outstanding — design fully specified and confirmed via clarifying questions (API shape, result-access style, custom-validation placement, file/test layout, flag-type scope).

## Notes

- Prior session (`threads-arg-testable-failure`) introduced `ThreadsArg.hh`/`.cc` and `ThreadsArgTest.cc`, currently sitting uncommitted in the working tree (`git status`: `header/ThreadsArg.hh`, `src/ThreadsArg.cc`, `test/` all untracked). This session replaces that uncommitted work rather than building on top of a committed baseline — no separate "revert" step needed, the new files simply supersede the old ones before anything is ever committed.
- No scout seed matched this feature's slug (`arg-parser-refactor`) in `.pending-seeds.json`; brainstorming started from zero per the user's direct request.

## Token Usage

- **Input:** 34
- **Output:** 12826
- **Cache read:** 1185477
- **Cache creation:** 140402
- **Total:** 1338739
