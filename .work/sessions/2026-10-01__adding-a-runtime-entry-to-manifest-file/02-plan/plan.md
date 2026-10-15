# Implementation Plan

**Session:** adding a runtime entry to manifest file
**Date:** 2026-10-01T08:23:35.348Z
**Estimated effort:** 2-3 hours

## Strategy

Add three wall-clock fields to `Manifest.json` (spec: `01-grill/resume.md`). `RunManifest` owns two `std::chrono::steady_clock` time points (process start, previous dump), and `RunAction` measures each run on the master. Ticket 01 ships the code and verifies it with a `sim.exe` run. Ticket 02 records the fields in the ADR, the `sim-output` skill and `CLAUDE.md`.

## Tickets Overview

| # | Ticket | Model |
|---|---|---|
| 01 | Runtime fields in `RunManifest`, `RunAction`, `sim.cc`, verified by a two-dump `sim.exe` run | sonnet |
| 02 | Document the fields (ADR 0005 addendum, `sim-output` skill, `CLAUDE.md`) | haiku |

## Sequencing Rationale

02 describes what 01 ships, including its final key names and placement, so 01 goes first.

## Risks & Mitigation

- **`EndOfRun_` flush with nothing pending:** `FlushIfPending` skips `Write`, so the previous-dump point is not updated. That is correct, because no dump happened.
- **MT:** `BeginOfRunAction`/`EndOfRunAction` also run on workers. The start time must be stored and read only on the master (`IsMaster()`), the same as `RecordRun`.
- **Run with 0 events:** `EndOfRunAction` returns early before `RecordRun`, so no entry and no timing. Unchanged.
- **Failed manifest open:** still update the previous-dump point (the dump happened), so the next dump's span starts from this one.

## Assumptions

- `MarkProcessStart()` is called before anything else in `main()`. If it is never called (no caller today besides `sim.cc`), the process-start point defaults to static-initialization time of `RunManifest.cc`, which is effectively process start.
- No unit test: timing is in Geant4-facing code. The project's test scope is pure logic only.

## Token Usage

- **Input:** 12
- **Output:** 4884
- **Cache read:** 472430
- **Cache creation:** 16595
- **Total:** 493921
