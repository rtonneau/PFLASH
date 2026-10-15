# Ticket 02: score-track-lengths

**Model:** opus
**Model (Jev):** opus-5.5 (confidence 0.74)
**Effort:** high

**Acceptance Criteria:**
- [ ] `PrimaryKiller` sets a per-thread "primary killed" flag when it kills track 1 (eLossMin / minKineticE branch and the bounding-box branch for track 1); the flag is reset at the start of each event.
- [ ] `TrackingAction::PostUserTrackingAction` collects per-event values: for track 1, `GetTrackLength()`, `GetVertexKineticEnergy()`, final KE and end reason (killed if flagged, else escaped if `GetNextVolume() == nullptr`, else stopped); for every other e- track, adds its length and +1 to all-generations totals and, if `GetParentID() == 1`, to first-generation totals.
- [ ] `EventAction::EndOfEventAction` commits one `TrackLengthTable::Row` per non-aborted event (with run ID and event ID) into the thread's live table, then resets the per-event values.
- [ ] `Run` holds a pointer to the live per-thread table with a master-owned fallback (same pattern as `fInteractionCounter`); `Run::Merge` merges it; `RunAction::BeginOfRunAction` clears it.
- [ ] `RunAccumulator` accumulates the table across runs (Accumulate signature extended, getter, cleared by `ClearAccumulated`); `test/RunAccumulatorTest.cc` and its CMake sources updated and passing.
- [ ] `RunAccumulatorMessenger::WriteAllAndReset` writes `TrackLengths.Txt` and `TrackLengths.csv` through `OutputDir::Resolve` and lists both in the dump's files.
- [ ] Serial and `--threads 2` builds both produce the files.

**Files to Touch:**
- `header/scoring/PrimaryKiller.hh`, `src/scoring/PrimaryKiller.cc`
- `header/actions/TrackingAction.hh`, `src/actions/TrackingAction.cc`
- `header/actions/EventAction.hh`, `src/actions/EventAction.cc`
- `header/actions/Run.hh`, `src/actions/Run.cc`, `src/actions/RunAction.cc`
- `header/scoring/RunAccumulator.hh`, `src/scoring/RunAccumulator.cc`, `test/RunAccumulatorTest.cc`
- `src/scoring/RunAccumulatorMessenger.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja --output-on-failure
# then from build/ (RelWithDebInfo), a smoke macro: 10 keV e-, /run/initialize, /scheduler/endTime 1 ms, /run/beamOn 2, /run/dumpDataAndResetToDir tl
./sim <smoke-macro> --dir ../.scratch/tests/2026-10-10__primary-and-secondary-tracklength-scorers/serial
```

Expected:
All ctest tests pass. `tl/TrackLengths.csv` has 2 data rows, `primaryEnd = stopped`, `primaryEkin0_keV = 10`, primary length of a few µm (order of the 2.5 µm CSDA range), `nSecondaryAll >= nSecondaryFirstGen` and `secondaryAll_nm >= secondaryFirstGen_nm`. Same checks with `--threads 2`.

**Notes:**

Where the per-event values live is the implementer's choice (e.g. a small per-thread struct owned by TrackingAction, read by EventAction through the run manager's user tracking action, or a thread_local in TrackLengthTable's Geant4-side caller). Keep `TrackLengthTable` itself kernel-free. Track 1 is the only primary (the gun fires one particle). Non-e- secondaries are ignored. `EventAction` must skip `event->IsAborted()` events, as `Run::RecordEvent` already does. Put the smoke macro in the session scratch dir, not in `macro/`.
