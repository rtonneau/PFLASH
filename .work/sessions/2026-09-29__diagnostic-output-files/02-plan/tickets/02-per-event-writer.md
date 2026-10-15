# Ticket 02: per-event-writer

**Model:** opus

**Acceptance Criteria:**
- [ ] `EventAction::BeginOfEventAction`, when `G4DNAChemistryManager::GetInstanceIfExists()` is non-null and before its `BeginOfEventAction` call:
  - calls `PreChemicalFiles::EnsureStagingDir(OutputDir::GetDirectory(), err)`, raising `G4Exception(..., "PreChemicalStagingDir", FatalException, ...)` on failure;
  - installs a fresh writer with `SetPhysChemIO(std::make_unique<G4PhysChemIO::FormattedText>())`;
  - calls `WriteInto(StagingDir(dir) + "/" + StagedFileName(runId, eventId))`, with `runId` from `G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID()`.
- [ ] `EventAction::EndOfEventAction` calls `SetPhysChemIO(nullptr)` after the chemistry manager's `EndOfEventAction`.
- [ ] Removed:
  - `TimeStepAction::DumpPreChemical` and its call in `EndProcessing`, the commented `WriteChemistryOutput`, and both header declarations;
  - `EventAction::WriteChemistryOutput`/`DumpPreChemical` and their declarations;
  - the `StackingAction::DumpPreChemical` declaration;
  - includes that become unused.
- [ ] The debug `G4cout` in `TimeStepAction::StartProcessing`/`EndProcessing` is gone. The end-time line survives as `DnaLogger` Debug.
- [ ] `sim` builds, and the smoke run produces one correct file per event.

**Files to Touch:**
- `src/actions/EventAction.cc`
- `header/actions/EventAction.hh`
- `src/chemistry/TimeStepAction.cc`
- `header/chemistry/TimeStepAction.hh`
- `header/actions/StackingAction.hh`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
# scratch macro build/macro/prechem3.in: /gun/particle e-, /gun/energy 10 keV, /run/beamOn 3 (no dump command)
cd build && ./sim prechem3.in --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t2
```

Expected:
- `t2/.pending_prechem/` holds exactly `PreChemical_run0_event0.txt`, `PreChemical_run0_event1.txt` and `PreChemical_run0_event2.txt`. The exit flush does not move them yet; that is ticket 03.
- Each non-empty file starts with the `#Parent ID` header.
- Each file's `Energy (eV)` column sums to more than 0 and at most 10000 eV, and the three sums differ.

**Notes:**

- Verified in Geant4 11.4.1 source:
  - `FormattedText::CloseFile()` returns early until the first record is written, which is why each event needs a fresh writer;
  - `SetPhysChemIO` is at `G4DNAChemistryManager.cc:91`, and the per-thread writer lives in `fpThreadData`;
  - `CreateWaterMolecule` and `CreateSolvatedElectron` check that the writer is non-null, so a null writer is safe.
- Include `G4PhysChemIO.hh`.
- The `StackingAction.cc` file has no definition of `DumpPreChemical`; only the declaration goes.
- Keep the `ReactionCounter` logic in `TimeStepAction::UserReactionAction` untouched.
- Write the scratch macro under `build/macro/` (untracked) and never edit `macro/beam.in`.
