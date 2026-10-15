# Session: diagnostic-output-files

**Date:** 2026-09-29T17:14:25.153Z
**Status:** Grill phase complete

## Problem Statement

Review findings F-002 and F-003 (`docs/reviews/2026-09-29-whole-project-review.md`).

- **F-002:** `TimeStepAction::EndProcessing` calls `G4DNAChemistryManager::WriteInto("output_event_N.txt")` at the *end* of event N. Geant4 11.4.1's `FormattedText::CloseFile()` returns early while no record has been written (`!fFileInitialized`), so the stream stays open. Event N+1's physico-chemical records land in event N's file, the next `open()` fails, and only one file per thread is ever written. The file also ignores the Dump prefix/subdir and is overwritten across Serial runs.
- **F-003:** `/chem/reaction/dump` is written from `DnaChemistryList::ConstructProcess`, which runs on the master and on every worker in MT, so all threads write the same path at the same time.

## Context & Constraints

- The user wants the file kept and always on, not removed.
- Geant4 11.4.1 API (verified in source): `G4DNAChemistryManager::SetPhysChemIO(std::unique_ptr<G4VPhysChemIO>)`, `WriteInto`, `CloseFile`. `G4DNAChemistryManager::Run()` calls `CloseFile()` after `Process()`. `FormattedText` writes its header lazily, on the first record.
- In MT the master assigns event IDs, which are unique within a run. Run IDs increase across `/run/beamOn` calls in one process.
- A Dump (`CONTEXT.md`) is the output unit. Its prefix/subdir is only known at `/run/dumpDataAndReset` / `/run/dumpDataAndResetToDir` time (Idle state, when all events are finished).
- Dead code to remove: `EventAction::WriteChemistryOutput`/`DumpPreChemical`, the `StackingAction::DumpPreChemical` declaration, and in `TimeStepAction` the commented `WriteChemistryOutput`, its unused declaration and the debug `G4cout` in `StartProcessing`/`EndProcessing` (use `DnaLogger` Debug where still useful).

## Success Metrics

- 3 events in Serial → 3 files `PreChemical_run0_event{0,1,2}.txt` in the Dump. Each file's H2O record count matches that event's pre-chemical main-list size, and its first `#Parent ID` block's energy sum is of the order of the beam energy.
- An MT run gives one file per event (not per thread), all moved into the Dump and listed in `Manifest.json` `files`.
- Two `/run/beamOn` calls in one Dump give distinct `run0_*` and `run1_*` files.
- The `/chem/reaction/dump` file is byte-identical between Serial and `--threads 4`, and its line count equals the number of bimolecular plus bulk reactions.
- Existing ctest suite still passes (Debug, `build-ninja/`).

## Architecture & Approach

- **Per-event writer:** in `EventAction::BeginOfEventAction`, for event E of run R, install a fresh `G4PhysChemIO::FormattedText` via `SetPhysChemIO` (the destructor closes the previous stream in every case), then `WriteInto(<outdir>/.pending_prechem/PreChemical_run<R>_event<E>.txt)`. Create the staging folder if it is missing. Remove the `WriteInto` call from `TimeStepAction::EndProcessing`.
- **Dump integration:** in `RunAccumulatorMessenger::DumpAndReset` (this covers both dump commands and the `EndOfRun_` safety-net flush), scan the staging folder and move each file to `<prefix>PreChemical_run<R>_event<E>.txt` (or into `<subdir>/`) through the `OutputDir` resolution. Add each moved file to the manifest's `files` list, individually.
- **Move failures:** overwrite an existing target. Any other failure is a `JustWarning`, and that file stays staged and is not listed.
- **Empty events** keep their empty file, so the number of files equals the number of events.
- **Stale staged files** from an aborted earlier process are swept into the first Dump, with no special handling.
- **F-003:** guard the dump with `if (!fReactionDumpFile.empty() && !G4Threading::IsWorkerThread())` in `DnaChemistryList::ConstructProcess`.
- **Docs:** update `run.outputs` in `.claude/.claude-project.json`, the `sim-output` skill and `CLAUDE.md` where they mention `output_event_*`. `CONTEXT.md` already has a "Pre-chemical file" entry, and ADR 0005 has an addendum.

## Assumptions & Trade-offs

- Scanning a staging folder instead of keeping a registry of files avoids cross-thread bookkeeping. The cost is a hidden folder in the output dir, and leftovers from a crash go into the next process's first Dump (user choice).
- Listing every file in the manifest makes it grow with the event count. The user accepted this to keep `files` consistent.
- The file is always on, so there is per-event file I/O cost even when nobody needs the file.
- The reaction-table dump stays outside the Dump prefix/subdir, as today. Only the thread guard is added.

## Open Questions

None.

## Notes

- Seed: scout report `scout-reports/review-2026-09-29-whole-project-review-2026-09-29T16-49-12-241Z.md` (F-002 High, F-003 Medium).
- Decision record: `docs/adr/0005-manifest-per-dump.md` addendum (2026-09-29). Glossary: `CONTEXT.md` "Pre-chemical file".

## Token Usage

- **Input:** 48
- **Output:** 13174
- **Cache read:** 1866050
- **Cache creation:** 36320
- **Total:** 1915592
