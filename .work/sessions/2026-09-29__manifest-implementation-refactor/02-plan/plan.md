# Implementation Plan

**Session:** Manifest implementation refactor
**Date:** 2026-09-29T13:57:18.376Z
**Estimated effort:** Half a day

## Strategy

Write `Manifest.json` from an ordered, format-neutral `DataNode` tree, so that adding or removing an entry means editing one `Add` line in `RunManifest.cc`. The generic `JsonWriter` serialises any tree and knows no keys. Spec: `.work/sessions/2026-09-29__manifest-implementation-refactor/01-grill/resume.md`.

Global constraints (every ticket):
- `DataNode` and `JsonWriter` use the standard library only and include nothing from Geant4 or the project apart from each other. Includes are rooted (`scoring/...`).
- `Manifest.json` keeps its current keys, key order and values; `schemaVersion` stays 1.
- Numbers use 12 significant digits, non-finite doubles become `null`, and integers print as integers.
- Tests use plain `assert` + CTest and run from `build-ninja/` (Debug). Each new test's `main` copies the MSVC CRT report-mode block from the existing tests.
- Code follows Geant4 style and the surrounding files.

Review focus (each pinned by a test):
1. A dump with no runs gives `"runs": []`, `totalEvents` 0 and `totalEnergyDeposit_eV` 0 (tickets 01 and 02).
2. Windows paths and quotes in `macro` / `outputDir*` are escaped correctly (ticket 01).
3. A NaN pH is written as `null` (ticket 01).
4. `threads: 4` stays an integer, `10.0` prints as `10`, and a large seed is exact (ticket 01).
5. Re-adding an existing key replaces its value in place, with no duplicate (ticket 01).

## Tickets Overview

1. `01-datanode-jsonwriter`: the portable `DataNode` tree and generic `JsonWriter`, plus `JsonWriterTest`.
2. `02-manifest-from-tree`: `RunAccumulator` stores run entries and an event total; `RunManifest` builds the manifest from `DataNode` (`Write` and the new `RecordRun`); `Beam` moves to `Run`; `ManifestData`/`ManifestWriter` are deleted.
3. `03-e2e-and-docs`: an end-to-end `sim` run that checks `Manifest.json`, plus updates to ADR 0005, `CLAUDE.md` and the `sim-output` skill.

## Sequencing Rationale

01 is self-contained and tested on its own. 02 has to switch the whole pipeline in one commit: `RunAction`, `RunAccumulator`, `RunManifest` and `Run` share the `RunRecord`/`Beam` types, so a partial switch would not compile. 03 needs the working binary from 02 and describes its final shape.

## Risks & Mitigation

- **`DataNode` constructor overloads could be ambiguous** (e.g. for `std::size_t` or `unsigned`). The call sites use `int`, `long`, `double` and `std::string`/`G4String`; cast explicitly where needed.
- **Totals now come from `RunAccumulator`** rather than being summed from the run entries, so `totalEnergyDeposit_eV` may differ from the old value only by floating-point summation order. Ticket 03 checks that the event total matches.
- **The null-beam branch and top-level key order are no longer unit-tested.** Ticket 03's end-to-end check verifies the key lists.
- **CMake target changes need a reconfigure** of `build-ninja/` and `build/` before building.

## Assumptions

- `EndOfRunAction` returns early for 0-event runs, so `RecordRun` never sees an empty run.
- `RecordRun` is called on the master thread only (inside the `IsMaster()` branch).
- `docs/visual/app-workflow-files.svg` isn't tracked by git and is left unchanged.

## Token Usage

- **Input:** 22
- **Output:** 19020
- **Cache read:** 1027585
- **Cache creation:** 34919
- **Total:** 1081546
