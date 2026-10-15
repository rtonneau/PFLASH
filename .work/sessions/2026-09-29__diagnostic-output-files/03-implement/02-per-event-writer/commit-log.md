# Ticket 02 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- `494d4f2` fix: write one pre-chemical file per event (F-002, ticket 02)

## Local Test Result

```
cmake --build build --config RelWithDebInfo --target sim   -> exit 0 (EventAction, TimeStepAction, StackingAction recompiled, sim.exe linked)
build/sim.exe prechem3.in --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t2
  (e-, 10 keV, /run/beamOn 3, no dump command)          -> exit 0, no G4Exception in log

t2/.pending_prechem/ contains exactly:
  PreChemical_run0_event0.txt  148260 B
  PreChemical_run0_event1.txt  147616 B
  PreChemical_run0_event2.txt  146530 B
Each starts with the "#Parent ID  Molecule  Elec Modif  Energy (eV) ..." header.

Energy (eV) column sums (H2O rows):
  event0: 591 rows, 7391.31 eV
  event1: 580 rows, 7368.33 eV
  event2: 572 rows, 7733.21 eV
All in (0, 10000] eV and pairwise different -> the Run()/CloseFile vs EndOfEventAction
ordering holds (G4DNAChemistryManager::Run() calls CloseFile() after Process(), inside
StackingAction::NewStage, before EndOfEventAction drops the writer).
EndOfRun_* flush files written in t2/; staged files not moved (ticket 03).
Logs: .scratch/tests/2026-09-29__diagnostic-output-files/02-build.log, 02-run.log
```

## Review Notes

- Writer lifecycle verified in Geant4 11.4.1 source: `SetPhysChemIO` (G4DNAChemistryManager.cc:91) replaces the per-thread writer; `WriteInto` opens the stream and resets `fFileInitialized`; `Run()` ends with `CloseFile()`; `FormattedText::CloseFile()` returns early before the first record, so `SetPhysChemIO(nullptr)` at end of event (destructor) is what guarantees the file of an event without records is closed.
- `G4DNAChemistryManager::BeginOfEventAction/EndOfEventAction` only forward to `G4MoleculeCounterManager`; ordering relative to the writer calls is free, kept as the ticket states.
- Removed `EventAction::WriteChemistryOutput/DumpPreChemical`, `TimeStepAction::DumpPreChemical` (+ call in `EndProcessing`, commented `WriteChemistryOutput`), all related declarations incl. `StackingAction::DumpPreChemical`, and now-unused includes (`G4ITTrackHolder`, `G4AnalysisManager`, `G4Threading`, `G4UnitsTable`, `G4RunManager`, `G4DNAChemistryManager`, `OutputDir` in TimeStepAction; `G4RunManager.hh` in EventAction.hh).
- `StartProcessing`/`EndProcessing` G4cout removed; end-of-chemistry global time kept as a DnaLogger Debug line. `UserReactionAction`/ReactionCounter untouched.
- Scratch macro at build/macro/prechem3.in (untracked); macro/beam.in not edited. Unit tests not re-run: no tested source changed.

## Time Spent

~0.3 hours

## Blockers / Challenges

None

## Token Usage

- **Input:** 30
- **Output:** 1966
- **Cache read:** 1040231
- **Cache creation:** 76725
- **Total:** 1118952
