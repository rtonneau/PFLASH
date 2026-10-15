# Ticket 03: beam-capture

**Acceptance Criteria:**
- [ ] `Run` stores a beam (`SetBeamIfUnset`, `HasBeam`, `GetBeam`) and the run's seed (`GetSeed`, taken in the constructor).
- [ ] `Run::Merge` copies a worker's beam onto the master run when the master has none.
- [ ] `PrimaryGeneratorAction::GeneratePrimaries` snapshots particle, energy (keV), position (um) and direction into the current `Run` once per run.
- [ ] `RunAction::EndOfRunAction` (master) adds a `ManifestData::RunRecord` (run ID, events, beam, seed, `energyDeposit_eV`) via `RunAccumulator::AddRunRecord`.
- [ ] `build/` (RelWithDebInfo) compiles.

**Files to Touch:**
- `header/actions/Run.hh`
- `src/actions/Run.cc`
- `src/actions/PrimaryGeneratorAction.cc`
- `src/actions/RunAction.cc`

**Verification Step:**

Run:
```bash
cmake --build build
```

Expected:
Build succeeds with no errors. Behaviour is verified by the ticket 04 smoke run.

**Notes:**

Plan Task 3 has the code. No unit test: Geant4-object code is verified by the smoke run, per project convention. If ticket 04 shows the seed does not follow `/random/setSeeds`, switch to `G4Random::getTheSeeds()[0]`.
