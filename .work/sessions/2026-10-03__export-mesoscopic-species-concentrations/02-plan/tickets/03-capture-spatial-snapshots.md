# Ticket 03: capture-spatial-snapshots

**Model:** opus

**Acceptance Criteria:**
- [ ] When `MesoSettings::Current().spatialOutput` is true, `MeshTotalsAction` also builds a `MesoSpatialFile::EventData` at each record time, in both `Record()` and `FinishRecording()`. Several record times passed in one call share one `Snapshot` (one entry in `snapshots`, several `records`).
- [ ] A Snapshot holds every cell from `mesh.const_begin()` to `const_end()`, empty ones included. Position is `std::get<1>(voxel).middlePoint() / nm`, `cellSize_nm` is `mesh.GetResolution() / nm`, and counts are per species column, 0 if absent.
- [ ] The species column list is built once per thread: every `G4MoleculeTable` configuration except userID `H2O` and userIDs ending in `(B)`, by `GetName()`, sorted. A mesh species outside the list (not expected) is skipped and logged once with a `DnaLogger` Warning.
- [ ] After `FinishRecording` in `CompartmentBased()`, if there are records, `TimeStepAction` calls `MesoSpatialFile::AppendEvent(MesoSpatialFile::StagedPath(OutputDir::GetDirectory()), species, data, err)`. It uses the current run ID and event ID. A failure is a `G4Exception` `JustWarning` (`MesoSpatialWriteFailed`). The per-event data is then cleared.
- [ ] With the switch off, no snapshot is built and no file is created; behaviour is unchanged.

**Files to Touch:**
- `src/chemistry/TimeStepAction.cc`
- `header/chemistry/TimeStepAction.hh` (only if a member is needed)

**Verification Step:**

Run (MSVC env; scratch macro in `build/macro/`, untracked: 10 keV e-, `/chem/meso/spatialOutput true`, `/run/initialize`, `/scheduler/endTime 1 ms`, `/run/beamOn 2`, no dump command):
```bash
cmake --build build --config RelWithDebInfo --target sim && cd build && ./sim spatial_smoke.in --dir ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t03 && ls ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t03/.pending_meso_spatial
```

Expected:
`SpeciesMesoSpatial.h5` is listed (the move into the dump is ticket 4), and `h5ls -r` shows `/run0/event0` and `/run0/event1` with snapshot groups. No `FatalException`.

**Notes:**

- The record-time logic is in `MeshTotalsAction::Record` / `FinishRecording`: reuse their loop and take one snapshot per call, not one per time.
- `StartRecording` must clear the event data.
- Run ID on the worker: `G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID()`. Event ID: `G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID()`.
- Positions are in the world frame (the mesh box is the chemistry boundary, centred at the origin).
