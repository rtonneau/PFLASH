# Implementation Plan

**Session:** diagnostic-output-files
**Date:** 2026-09-29T17:41:16.328Z
**Estimated effort:** 1 day

## Strategy

Spec: `01-grill/resume.md`; ADR `docs/adr/0005-manifest-per-dump.md` (addendum 2026-09-29); glossary `CONTEXT.md` "Pre-chemical file".

- A pure `PreChemicalFiles` module (standard library only, unit-tested) names, stages and moves the per-event files.
- `EventAction` gives each event its own `G4PhysChemIO::FormattedText` through `SetPhysChemIO`, writing into `<outdir>/.pending_prechem/PreChemical_run<R>_event<E>.txt`. It releases the writer at end of event.
- `RunAccumulatorMessenger::WriteAllAndReset` (shared by both dump commands and the `EndOfRun_` flush) moves the staged files into the Dump and lists each one in `Manifest.json` `files`.
- F-003: a worker-thread guard on the `/chem/reaction/dump` write.

Global constraints (every ticket):
- Staging folder: `<outdir>/.pending_prechem/`, or `.pending_prechem/` in cwd when no `--dir` is set. It ignores the prefix and subdir.
- Name inside the Dump: `<prefix>PreChemical_run<R>_event<E>.txt`, or the same name in `<subdir>/`. No thread ID.
- One file per event, including an empty one.
- Manifest order is numeric by (run, event).
- An existing target is overwritten. Any other move failure is a `JustWarning`, and the file stays staged and is not listed.
- Stale staged files are swept into the first Dump.
- Geant4 conventions and `DnaLogger` for logging. Unit tests run from `build-ninja/` (Debug); smoke runs use `build/` (RelWithDebInfo) with a 10 keV e- gun. Commands run inside the MSVC env wrapper from `.claude/geant4-instructions.md`.

## Tickets Overview

1. `01-prechemical-files-module`: pure naming, staging and move helpers, plus `PreChemicalFilesTest`.
2. `02-per-event-writer`: one writer per event in `EventAction`; remove the old `WriteInto` path and the dead code.
3. `03-move-into-dump`: dump moves staged files and lists them in the manifest; docs updated.
4. `04-reaction-dump-master-only`: F-003 thread guard.

## Sequencing Rationale

- 01 provides the helpers that 02 (names, staging folder) and 03 (move) use.
- 02 must land before 03 can be smoke-tested end to end.
- 04 is independent. It comes last because it is the smallest and touches a different file.

## Risks & Mitigation

- **The last event's file is still open at dump time**, so the move fails on Windows. Mitigation: `SetPhysChemIO(nullptr)` in `EndOfEventAction`. The MT smoke in 03 checks that the staging folder ends up empty.
- **`event10` sorts before `event2`.** Mitigation: numeric sort, pinned by the `MoveStagedOrdersNumerically` test.
- **Unrelated files in the staging folder.** Mitigation: only exact name matches are moved (`MoveStagedIgnoresForeignFiles`).
- **Two `beamOn` calls in one Dump.** Mitigation: the run ID is in the name. The 03 Serial smoke uses 2 + 1 events.
- **No staging folder at dump time.** Mitigation: the move is a no-op (`MoveStagedMissingDirIsEmpty`).
- **The `SetPhysChemIO` lifetime or the order of `G4DNAChemistryManager::Run()` versus `EndOfEventAction`** could differ from what the plan expects. Mitigation: 02's smoke checks per-file energy sums (each at most 10 keV, and the three sums differ).

## Assumptions

- The Geant4 11.4.1 chemistry stage (`G4DNAChemistryManager::Run`) completes before the user `EndOfEventAction`.
- `G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID()` on a worker thread matches the master run ID, and event IDs are unique within a run in MT.
- `std::filesystem::rename` within one output folder is atomic enough. The ticket still removes an existing target explicitly before the rename.

## Token Usage

- **Input:** 30
- **Output:** 19018
- **Cache read:** 1697082
- **Cache creation:** 34680
- **Total:** 1750810
