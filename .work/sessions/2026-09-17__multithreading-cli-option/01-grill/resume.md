# Session: multithreading-cli-option

**Date:** 2026-09-17T09:37:14.784Z
**Status:** Grill phase complete

## Problem Statement

`sim.cc` hardcodes `G4RunManagerType::Serial` at compile time; running multithreaded requires editing and rebuilding the source (flipping the commented-out `G4RunManagerType::MT` line). There is no way to opt into multithreading, or choose a worker-thread count, from the command line at run time.

## Context & Constraints

- **Current behavior:** `main()` in `sim.cc` sets `runManagerType` from a hardcoded constant (`Serial`, with `MT` available only by uncommenting a line and rebuilding). When `MT` was hardcoded, the batch path set a fixed default of 4 worker threads via `runManager->SetNumberOfThreads(4)`. `argv[1]`, if present, is treated as the macro file name (`"macro/" + argv[1]`); there was no other CLI argument handling.
- **Pain point:** Switching between Serial (reproducible) and MT (faster) runs required a source edit and rebuild; no way to pick a thread count without another edit.
- **Dependencies:** Must keep the default Serial behavior unchanged (reproducibility is relied on elsewhere — see the RNG-seed comment in `sim.cc` and the project's testing-infrastructure work). Must keep the existing `argv[1]` = macro-file convention working unchanged. A macro can still override thread count later via `/run/numberOfThreads N` before `/run/initialize`, per existing comment — new logic must not break that.
- **Tech stack:** Geant4 11.4 (`G4RunManagerFactory`, `G4RunManagerType`, `G4Threading::G4GetNumberOfCores()` — confirmed against Geant4 source and the shipped `chem1.cc` DNA example, per this project's "never guess Geant4-DNA API behavior" rule), the project's own `DnaLogger` for diagnostics.

## Success Metrics

- `./sim beam.in` (no flag) still runs Serial, unchanged from today.
- `./sim beam.in --threads N` runs MT with exactly N worker threads (verified via `G4WT<i>` log prefixes).
- Invalid input (missing value, non-numeric, `<= 0`, or N greater than `G4Threading::G4GetNumberOfCores()`) prints a clear error and exits 1 without starting a run.

## Architecture & Approach

Single-file change in `sim.cc`, no new classes or files:

- New static helper `ParseThreadsArg(argc, argv)` scans `argv` for a `--threads N` flag (the macro file, if given, must still be `argv[1]` and precede the flag — order-independent parsing was considered and explicitly declined in favor of the simpler fixed-position approach). Returns the validated thread count, or `0` meaning "not requested."
- Validates, in order: flag has a following value; value is a clean positive integer (`std::strtol`, must consume the whole string); value does not exceed `G4Threading::G4GetNumberOfCores()`. Any failure logs via `DnaLogger` (level forced to `Error` first, since it defaults to `Quiet` before any macro can raise it) and calls `exit(1)`.
- `main()` picks `runManagerType = MT` only when `ParseThreadsArg` returns `> 0`, otherwise `Serial` (today's default, unchanged). The old hardcoded batch default of 4 threads is replaced by the validated, user-supplied count.

## Assumptions & Trade-offs

- Not doing order-independent flag parsing (`--threads` before the macro file) — the macro file must come first if given. Accepted trade-off for simplicity; can be revisited if it turns out to matter in practice.
- Not adding a "clamp and warn" fallback for over-large thread counts — over-requesting cores is a hard error (exit 1), per explicit choice over silently clamping.
- Not touching GUI mode (`useGUI` branch) — it already forces `SetNumberOfThreads(1)` unconditionally regardless of this flag, which is unchanged and out of scope.
- This is a **bounded** change (existing flow being extended, not a new subsystem), so per the brainstorming skill's bounded path this session skipped the formal plan/tickets flow and went straight from an in-chat design approval to implementation.

## Open Questions

None outstanding — implementation is complete and manually verified (see Notes).

## Notes

- Implemented and verified directly in this session (bounded path — no `/gps plan`/`/gps ticket` cycle was used):
  - Missing value, non-numeric value, `0`, and an over-large thread count (99999 against 20 available cores) all correctly error and exit 1.
  - Default (no flag) confirmed Serial, no worker threads spawned.
  - `--threads 2` confirmed MT with exactly two worker threads (`G4WT0`/`G4WT1` log prefixes).
- Building this project always requires the MSVC x64 dev environment loaded in the shell first (plain shells fail on missing STL headers like `cstddef`/`complex` even though `cmake` configures fine) — this was previously undocumented and has now been added to the project's `CLAUDE.md` `Build and Run` section, along with a fix to two invocation examples that had a `macro/` path duplicated (`sim.cc` already prepends `macro/` to the argument itself).
- No commit has been made yet for the `sim.cc` change.
