# Ticket 02: irtsyn-meso-spike

**Model:** opus

**Acceptance Criteria:**
- [ ] SBS is gone:
  - `DnaChemistryList::ConstructTimeStepModel` registers `G4DNAIndependentReactionTimeModel` only.
  - `G4DNABrownianTransportation` stays registered.
  - `PhysicsList` sets `G4ChemTimeStepModel::IRT_syn`.
  - Any other value of `/process/chem/TimeStepModel` is a fatal `G4Exception` naming IRT_syn as the only supported model.
  - Stale SBS comments in these files are fixed.
- [ ] `TimeStepAction` owns a `std::unique_ptr<G4DNAEventScheduler>` (one per thread). When the scheduler global time first reaches the hand-over time (constant 5 ns in this ticket), it calls `SetStartTime(globalTime)`, `SetChangeMesh(true)`, `Initialize(*chemistryBoundary, pixel)` and `Run()`, following UHDR `TimeStepAction::CompartmentBased`. `EndProcessing` resets the scheduler.
- [ ] `pixel` is the power of 2 closest to `2 * halfBox / 6.25 nm`, **capped at 65536** (temporary inline computation; ticket 04 replaces it). *Amended after the first spike was BLOCKED:* starting at 131072 pixels, the first coarsening overflows a 32-bit int in `G4DNAMesh::ConvertIndex` (`index.x * pixels / xmax`), giving fatal `G4DNAMesh013` at t ≈ 144 ns. Starting at 65536 or fewer is safe (65535 × 32768 < 2³¹). On the default 1 mm box the initial cell is therefore 15.26 nm (user decision 2026-10-02: keep the 1 mm box, accept the coarser cell).
- [ ] The two integration fixes from the first spike attempt stay. The thread-local `G4MoleculeCounterManager` is muted during the mesoscopic `Initialize`/`Run`, which avoids `TIME_DONT_MATCH`, so `Species.*` is particle-stage only. A record-time grid is registered with `AddTimeToRecord` (temporary, one point per decade), with `ResetCounter` and `ParticleBasedCounter` as in UHDR, which avoids an uninitialised-iterator crash in `RecordTime`.
- [ ] At `/dnaLogger/verbose Debug`, the total molecule count is logged just before the hand-over, right after the mesoscopic `Initialize`, and at each mesh change (via a `G4UserMeshAction`).
- [ ] Spike runs on the default 1 mm box (10 keV e-, `/run/beamOn 2`, PureWater, no scavenger), once Serial and once `--threads 2`:
  - exit 0 and the success markers present;
  - `Species.Txt` non-empty;
  - totals consistent through the hand-over and every mesh change: only reactions change them, and no jump of more than the reaction count is allowed between two consecutive logged points.
- [ ] Findings go to `.scratch/tests/2026-10-02__irt-syn-mesoscopic/spike-findings.md`: the pixel count used, every mesh change (pixels and cell size), the totals, the wall time per event to 1 µs and to 1 s, and the overflow verdict. If totals drift, or the run crashes in `Voxelizing` / `ReVoxelizing`, the ticket ends BLOCKED with these findings and does not work around it.

**Files to Touch:**
- `src/chemistry/DnaChemistryList.cc`, `header/chemistry/DnaChemistryList.hh`
- `src/physics/PhysicsList.cc`
- `src/chemistry/TimeStepAction.cc`, `header/chemistry/TimeStepAction.hh`
- `src/actions/ActionInitialization.cc` (only if `TimeStepAction` needs the chemistry world)

**Verification Step:**

Run (PowerShell, MSVC env; macros are scratch, copied into `build/macro/`):
```bash
cmake --build build --target sim
cd build; ./sim 02-spike.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/02-serial > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/02-serial.log 2>&1
./sim 02-spike.in --threads 2 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/02-mt > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/02-mt.log 2>&1
grep -E "meso|pixel|total" ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/02-serial.log | head -40
```

Expected:
Both runs exit 0, with `dumped and reset` present and no `EEEE`. The log shows the hand-over at 5 ns, the initial pixel count, each mesh change, and molecule totals that only change by reactions. The `--threads 2` run logs one hand-over per event per worker.

**Notes:**

Scratch macro `02-spike.in`: `/dnaLogger/verbose Debug`, `/run/initialize`, `/scheduler/endTime 1 s`, 10 keV e-, `/run/beamOn 2`, `/run/dumpDataAndReset`. Patterns to copy: UHDR `TimeStepAction.cc` (constructor, `UserPostTimeStepAction`, `CompartmentBased`, `EndProcessing`) and `G4EmDNAChemistry_option3::ConstructTimeStepModel` (IRT_syn branch). The chemistry boundary is `DnaChemistryWorld::GetChemistryBoundary()`, reached through the run manager's detector as `DnaChemistryList::ChemistryWorld` does. Don't touch `macro/*.in` here (ticket 06). The bulk reactions stay as today (`G4DNAScavengerProcess` only) until ticket 03.
