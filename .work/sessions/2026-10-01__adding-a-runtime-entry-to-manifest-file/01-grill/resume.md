# Session: adding a runtime entry to manifest file

**Date:** 2026-10-01T08:18:16.930Z
**Status:** Grill phase complete

## Problem Statement

`Manifest.json` records what produced a dump's data (Chemistry, environment, beams, seeds, totals) but not how long it took. Add wall-clock runtime entries so each dump states the time spent per `/run/beamOn` and the time the dump covers.

## Context & Constraints

- `RunManifest::Write` runs at every dump, including the `EndOfRun_` safety-net flush, which `sim.cc` calls before `theTimer->Stop()`. Total process time is therefore unknown when any manifest is written; only elapsed time up to the dump can be recorded.
- Per-run entries are built by `RunManifest::RecordRun` from master `RunAction::EndOfRunAction`.
- ADR 0005: `RunManifest` is the only place listing manifest entries; units go in the key; adding fields does not bump `schemaVersion`.

## Success Metrics

- Every `Manifest.json` (manual dumps and `EndOfRun_`) has `elapsedSinceStart_s` and `elapsedSincePreviousDump_s` at top level, and `wallTime_s` in each `runs[]` entry, all > 0.
- `elapsedSincePreviousDump_s <= elapsedSinceStart_s`; for a first dump they are equal.
- Sum of a dump's `runs[].wallTime_s` <= its `elapsedSincePreviousDump_s`.
- Verified with a 10 keV e-, `/run/beamOn 2` macro that dumps twice.

## Architecture & Approach

Wall-clock only (`std::chrono::steady_clock`), in seconds.

| Key | Placement | Span |
|---|---|---|
| `elapsedSinceStart_s` | top level, right after `timestamp` | start of `main()` → this dump |
| `elapsedSincePreviousDump_s` | top level, right after the above | previous dump (or process start) → this dump |
| `wallTime_s` | each `runs[]` entry, after `seed` | master `BeginOfRunAction` → master `EndOfRunAction` (event loop, chemistry, MT merge; excludes run initialization) |

- `RunManifest::MarkProcessStart()` called at the top of `main()` (next to the `G4Timer` start). `RunManifest` keeps the process-start and previous-dump time points; `Write` updates the previous-dump point, including when the manifest file cannot be opened (the dump still happened).
- `RunAction` stores a start time point in `BeginOfRunAction` (master only); `RecordRun(run, wallTime_s)` adds `wallTime_s` to the run entry.
- Docs: addendum to `docs/adr/0005-manifest-per-dump.md` (what each time covers, wall clock only, why no total process time), field list in `.claude/skills/sim-output/SKILL.md`, `RunManifest`/`RecordRun` lines in `CLAUDE.md`.

## Assumptions & Trade-offs

- No CPU time: summed over MT threads it is misleading and the master's CPU time excludes workers.
- Run wall time excludes run initialization (physics tables on first beamOn); `elapsedSincePreviousDump_s` still captures it.
- No new unit test: timing lives in Geant4-facing code with no existing unit tests; verification is a `sim.exe` run.
- `schemaVersion` stays 1.

## Open Questions

None.

## Notes

No new `CONTEXT.md` term: these are output fields, not domain concepts.

## Token Usage

- **Input:** 38
- **Output:** 7885
- **Cache read:** 1074549
- **Cache creation:** 21398
- **Total:** 1103870
