# Ticket 02: energy-deposit-output

**Acceptance Criteria:**
- [ ] `RunAction::EndOfRunAction` (in `src/RunAction.cc`) writes `masterRun->GetSumDose()` to `OutputDir::Resolve("EnergyDeposit.Txt")` as a human-readable line using `G4BestUnit(..., "Energy")`.
- [ ] After writing, a `DnaLogger::Print(DnaLogger::Level::Info, ...)` line is logged, matching the existing `[RunAction] ... written (...)` style used for species/reactions.
- [ ] `.claude/.claude-project.json`'s `run.outputs` array includes `"EnergyDeposit.Txt"` and `run.successMarkers` includes `"[RunAction] energy deposit written"`.
- [ ] A smoke run (25 keV e-, `/run/beamOn 10`, single beamOn) produces a non-empty `EnergyDeposit.Txt` with a plausible nonzero energy value, and the log contains the new success marker with exit code 0 and no `EEEE`/`FatalException`.

**Files to Touch:**
- `src/RunAction.cc`
- `.claude/.claude-project.json`

**Verification Step:**

Run (PowerShell, MSVC env per `.claude/geant4-instructions.md` section 1):
```powershell
cmake --build build --config RelWithDebInfo --target sim
```
then, from `build/`, with a scratch macro `build/macro/smoke_energy.in` (untracked; content below), run:
```powershell
./sim.exe smoke_energy.in --dir ../.scratch/tests/2026-09-23__physics-stage-output/task-02
```

Expected:
Build succeeds; run exits 0; log contains `[RunAction] energy deposit written (EnergyDeposit.Txt)`; `.scratch/tests/2026-09-23__physics-stage-output/task-02/EnergyDeposit.Txt` exists, non-empty, with a nonzero `G4BestUnit`-formatted energy value.

**Notes:**

No new scoring code — `Run::fSumEne`/`Run::GetSumDose()` (`header/Run.hh:25`) already accumulates `G4Step::GetTotalEnergyDeposit()` per step in the World volume via `ScoreSpecies::ProcessHits`, and is already correctly merged across worker threads in `Run::Merge`. `G4UnitsTable.hh`, `OutputDir.hh`, `DnaLogger.hh`, and `<fstream>` are already included in `src/RunAction.cc` — no new includes required.

Insert this block in `src/RunAction.cc`, inside `EndOfRunAction`'s `if (IsMaster())` branch, right after the existing reaction-counts block (after the `DnaLogger::Print(...)` call that logs `"[RunAction] reaction counts written ..."`, still before that block's closing `}`):

```cpp
        // Write the total energy deposited in the simulation volume (merged
        // across worker threads by Run::Merge; accumulated per-step by
        // ScoreSpecies::ProcessHits into Run::fSumEne).
        std::ofstream energyOut(OutputDir::Resolve("EnergyDeposit.Txt"));
        energyOut << "Total energy deposited in simulation volume: "
                  << G4BestUnit(masterRun->GetSumDose(), "Energy") << "\n";
        energyOut.close();

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] energy deposit written (EnergyDeposit.Txt)");
```

Scratch macro `build/macro/smoke_energy.in` (untracked — do not edit tracked macros):

```
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info

/process/dna/e-SolvationSubType Ritchie1994
/process/chem/TimeStepModel SBS

/run/initialize

/gun/particle e-
/gun/energy 25 keV

/run/beamOn 10
```
