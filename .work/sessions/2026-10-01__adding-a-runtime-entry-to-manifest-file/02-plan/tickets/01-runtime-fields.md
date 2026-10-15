# Ticket 01: runtime-fields

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `RunManifest::MarkProcessStart()` exists, is declared in `header/scoring/RunManifest.hh` with a doc comment, and is the first statement after `DnaLogger::SetLevel` in `main()` (`sim.cc`), before the `G4Timer` start.
- [ ] `RunManifest::RecordRun(const Run &run, double wallTime_s)` replaces `RecordRun(const Run &)`. It adds `"wallTime_s"` as the last key of the run entry, after `"seed"`.
- [ ] `RunAction` has a `std::chrono::steady_clock::time_point fRunStart` member, set in `BeginOfRunAction` when `IsMaster()`. `EndOfRunAction` passes `duration<double>(now - fRunStart).count()` to `RecordRun`.
- [ ] `RunManifest::Write` adds `"elapsedSinceStart_s"` (process start → now) and `"elapsedSincePreviousDump_s"` (previous dump, or process start if none → now) right after `"timestamp"`, using one `now` taken at the start of `Write`. It then sets the previous-dump point to that `now`, also on the failed-open path.
- [ ] Wall clock only (`std::chrono::steady_clock`). `schemaVersion` stays 1. No other manifest key changes.
- [ ] `build/` (RelWithDebInfo) builds without new warnings. `ctest` in `build-ninja/` still passes.
- [ ] A two-dump run gives two manifests where every field is > 0, `elapsedSincePreviousDump_s <= elapsedSinceStart_s` (equal on the first dump), and the sum of `runs[].wallTime_s` <= `elapsedSincePreviousDump_s`.

**Files to Touch:**
- `header/scoring/RunManifest.hh`
- `src/scoring/RunManifest.cc`
- `header/actions/RunAction.hh`
- `src/actions/RunAction.cc`
- `sim.cc`

**Verification Step:**

Build `build/` and `build-ninja/` following `.claude/geant4-instructions.md`, then run `ctest --test-dir build-ninja --output-on-failure`. Write the scratch macro `build/macro/runtime_check.in` (untracked): copy the setup lines of `macro/beam.in` up to and including `/run/initialize`, then:

```text
/gun/particle e-
/gun/energy 10 keV
/run/beamOn 2
/run/dumpDataAndResetToDir dumpA
/run/beamOn 2
/run/dumpDataAndResetToDir dumpB
```

Run (from `build/`):
```bash
./sim runtime_check.in --dir ../.scratch/tests/2026-10-01__adding-a-runtime-entry-to-manifest-file/out
```

Expected:
`The simulation took` printed, no `FatalException`. `out/dumpA/Manifest.json` and `out/dumpB/Manifest.json` both have `elapsedSinceStart_s` and `elapsedSincePreviousDump_s` right after `timestamp`, and `wallTime_s` last in each `runs[]` entry. In dumpA the two elapsed values are equal. In dumpB `elapsedSincePreviousDump_s` < `elapsedSinceStart_s`. In both, the sum of `wallTime_s` <= `elapsedSincePreviousDump_s`. No `EndOfRun_Manifest.json` (nothing pending at exit).

**Notes:**

- `Write` already takes a timestamp via `Timestamp()`. Keep it, and take the new steady-clock `now` separately (the system clock is for the date, the steady clock for durations).
- State lives in `RunManifest.cc`'s anonymous namespace next to `gMacroName`, e.g. `gProcessStart` (initialized to `steady_clock::now()` so an uncalled `MarkProcessStart` still gives a sane value) and `std::optional<time_point> gPreviousDump`.
- `RunAction.cc`: set `fRunStart` at the top of `BeginOfRunAction` under `if (IsMaster())`, before chemistry setup, so the span is Begin→End as the spec says. The existing `RecordRun` call is already inside `if (IsMaster())`.
- Match the file's style (4-space indent in `RunAction.cc`, 2-space in `RunManifest.cc`).
- Commit: `feat: record wall-clock runtimes in Manifest.json`.
