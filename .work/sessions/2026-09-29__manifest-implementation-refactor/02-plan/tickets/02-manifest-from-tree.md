# Ticket 02: manifest-from-tree

**Acceptance Criteria:**
- [ ] `RunAccumulator` API:
  - `void Accumulate(G4double energy, long events, const ReactionCounter &, const PhysicsInteractionCounter &)`
  - `long GetAccumulatedEvents()`
  - `void AddRunEntry(const DataNode &entry)` (does not set the pending flag)
  - `const std::vector<DataNode> &GetRunEntries()`

  `ClearAccumulated` also resets the event total and the run entries. `AddRunRecord`/`GetRunRecords` are removed.
- [ ] `RunManifest::RecordRun(const Run &run)` (class `Run` forward-declared in the header) builds one run entry, with keys in order: `run` (int), `events` (long), `particle`, `beamEnergy_keV`, `position_um` [3], `direction` [3], `energyDeposit_eV` (`GetSumDose()/eV`), `seed` (long). When `!run.HasBeam()`, the four beam keys are `DataNode()` (null). It stores the entry via `RunAccumulator::AddRunEntry`.
- [ ] `RunManifest::Write` builds the root with `DataNode::MakeObject()` and `Add` calls, in order:
  - `schemaVersion` (1), `timestamp`, `geant4Version`, `macro`, `chemistry`
  - `scavengers` (array of `{species, molarity_M}`), `pH`, `chemistryEndTime_ns`, `runMode`, `threads`
  - `outputDirAsConfigured`, `outputDirAbsolute`, `prefix`, `subdir`
  - `totalEvents` (`GetAccumulatedEvents()`), `totalEnergyDeposit_eV` (`GetAccumulatedEnergy()/eV`), `files`
  - `runs` (array of the `GetRunEntries()` entries)

  It then calls `JsonWriter::Write`. A failed open is still a `JustWarning`.
- [ ] `RunAction::EndOfRunAction` calls `Accumulate(masterRun->GetSumDose(), nofEvents, …)` and `RunManifest::RecordRun(*masterRun)` in place of the 8 `RunRecord` lines.
- [ ] `Beam` is nested in `Run` (`Run::Beam`, unchanged members). `PrimaryGeneratorAction` uses `Run::Beam`.
- [ ] `header/scoring/ManifestData.hh`, `header/scoring/ManifestWriter.hh`, `src/scoring/ManifestWriter.cc` and `test/ManifestWriterTest.cc` are deleted; `CMakeLists.txt` has no `ManifestWriterTest`, and `RunAccumulatorTest` also compiles `src/scoring/DataNode.cc`.
- [ ] `test/RunAccumulatorTest.cc`:
  - every `Accumulate` call takes the `events` argument;
  - `TestClearAccumulatedResetsEverything` asserts `GetAccumulatedEvents() == 0`;
  - new `TestAccumulateSumsEventsAcrossCalls`: events 2 then 3 → 5;
  - `TestRunEntriesAccumulateAndClear`: entries `MakeObject().Add("run", 3)` and `…Add("run", 4)` → `size() == 2`, first member integers 3 and 4, and `empty()` after `ClearAccumulated`;
  - `TestAddRunEntryDoesNotSetPendingFlag`.
- [ ] Header comments in `RunAccumulator.hh` and `RunManifest.hh` no longer mention `ManifestData`/`ManifestWriter`.

**Files to Touch:**
- `header/actions/Run.hh`
- `src/actions/PrimaryGeneratorAction.cc`
- `src/actions/RunAction.cc`
- `header/scoring/RunAccumulator.hh`
- `src/scoring/RunAccumulator.cc`
- `header/scoring/RunManifest.hh`
- `src/scoring/RunManifest.cc`
- `test/RunAccumulatorTest.cc`
- `CMakeLists.txt`
- delete: `header/scoring/ManifestData.hh`, `header/scoring/ManifestWriter.hh`, `src/scoring/ManifestWriter.cc`, `test/ManifestWriterTest.cc`

**Verification Step:**

Run (inside the MSVC environment, after reconfiguring both build dirs):
```bash
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure
cmake --build build --config RelWithDebInfo --target sim
grep -rnE "ManifestData|ManifestWriter|RunRecord" src header test CMakeLists.txt
```

Expected:
- every ctest test passes, and none is named `ManifestWriterTest`;
- `sim` builds with 0 errors;
- grep prints nothing.

**Notes:**

TDD: update `RunAccumulatorTest` first and confirm it fails to compile. `EndOfRunAction` already returns early when `nofEvents == 0` and calls `RecordRun` only inside `IsMaster()`. Commit: `refactor: build Manifest.json from a DataNode tree, drop ManifestData/ManifestWriter`.
