# Ticket 03: physical-interaction-wiring

**Acceptance Criteria:**
- [ ] `header/SteppingAction.hh` + `src/SteppingAction.cc` exist: a `G4UserSteppingAction` subclass that, in `UserSteppingAction(const G4Step*)`, records `step->GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName()` into an owned `PhysicsInteractionCounter` only when the process name contains `"G4DNA"` (null-checked; `Transportation` and other non-DNA steps excluded).
- [ ] `SteppingAction` is registered via `SetUserAction(new SteppingAction())` in `ActionInitialization::Build()` (worker/serial thread only, not `BuildForMaster()` — same placement as `TimeStepAction`/chemistry setup).
- [ ] `header/Run.hh`/`src/Run.cc` pick up the live counter from `G4RunManager::GetRunManager()->GetUserSteppingAction()` (via `dynamic_cast`+`const_cast`, mirroring the existing `TimeStepAction`/`ReactionCounter` pickup), falling back to an owned instance on the master thread; `Run::Merge` merges the worker's counter into the master's and clears the worker's copy afterward.
- [ ] `RunAction::EndOfRunAction` writes `PhysicsInteractions.Txt` (via `WriteAscii`) and `PhysicsInteractions.csv` (via `WriteCsv`) through `OutputDir::Resolve`, clears the counter, and logs a `[RunAction] physical interaction counts written (PhysicsInteractions.Txt / PhysicsInteractions.csv)` line via `DnaLogger`.
- [ ] `.claude/.claude-project.json`'s `run.outputs` includes `"PhysicsInteractions.Txt"` and `"PhysicsInteractions.csv"`; `run.successMarkers` includes `"[RunAction] physical interaction counts written"`.
- [ ] A smoke run (25 keV e-, `/run/beamOn 10`) produces both files with plausible nonzero counts for processes like `e-_G4DNAIonisation`/`e-_G4DNAExcitation`, and no `Transportation` row in either file.

**Files to Touch:**
- `header/SteppingAction.hh`
- `src/SteppingAction.cc`
- `header/Run.hh`
- `src/Run.cc`
- `src/RunAction.cc`
- `src/ActionInitialization.cc`
- `.claude/.claude-project.json`

**Verification Step:**

Run:
```powershell
cmake --build build --config RelWithDebInfo --target sim
```
then, from `build/`, reusing Ticket 02's scratch macro (`build/macro/smoke_energy.in`):
```powershell
./sim.exe smoke_energy.in --dir ../.scratch/tests/2026-09-23__physics-stage-output/task-03
```

Expected:
Build succeeds; run exits 0; log contains both `[RunAction] energy deposit written ...` and `[RunAction] physical interaction counts written ...`; `.scratch/tests/2026-09-23__physics-stage-output/task-03/PhysicsInteractions.Txt` has lines like `e-_G4DNAIonisation    count = N` with plausible nonzero `N`; `PhysicsInteractions.csv` has header `label,count` plus matching rows; neither file has a `Transportation` row.

**Notes:**

Depends on Ticket 01's `PhysicsInteractionCounter` class — build/run that ticket first. This ticket is deliberately not split further: `SteppingAction` alone has no independently observable behavior until it's wired through `Run`/`RunAction`, so the smoke-run verification only makes sense once all pieces are connected. `PhysicsInteractionCounter.hh` is pulled in transitively via `Run.hh`'s new include, so `RunAction.cc` needs no new include for it.

The `"G4DNA"` substring filter was verified against `source/physics_lists/constructors/electromagnetic/src/G4EmDNABuilder.cc` in the Geant4 v11.4.1 source (`C:\DEV\GEANT4\geant4-v11.4.1-source`): every discrete DNA physics process `G4EmDNAPhysics` registers is named `<particle>_G4DNA<Type>` (e.g. `e-_G4DNAIonisation`, `e-_G4DNAExcitation`, `e-_G4DNAElastic`, `e-_G4DNAVibExcitation`, `e-_G4DNAAttachment`).

`header/SteppingAction.hh`:

```cpp
/// \file SteppingAction.hh
/// \brief Definition of the SteppingAction class
///
/// Counts physical (pre-chemistry) interaction firings by process name,
/// keyed the same way as G4Step::GetPostStepPoint()->GetProcessDefinedStep()
/// reports them, so PhysicsInteractions.Txt/.csv read directly off Geant4's
/// own process names. Only discrete physics interactions are counted --
/// steps whose process name doesn't contain "G4DNA" (e.g. Transportation)
/// are ignored. The "G4DNA" substring holds for every process
/// G4EmDNAPhysics registers (verified against source: e-_G4DNAIonisation,
/// e-_G4DNAExcitation, e-_G4DNAElastic, e-_G4DNAVibExcitation,
/// e-_G4DNAAttachment, ...), so this filter transposes to any
/// G4EmDNAPhysics-based project by copy-paste.

#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "PhysicsInteractionCounter.hh"

class G4Step;

class SteppingAction : public G4UserSteppingAction
{
public:
  SteppingAction() = default;
  ~SteppingAction() override = default;

  void UserSteppingAction(const G4Step *step) override;

  PhysicsInteractionCounter &GetInteractionCounter() { return fInteractionCounter; }

private:
  PhysicsInteractionCounter fInteractionCounter;
};

#endif  // SteppingAction_h
```

`src/SteppingAction.cc`:

```cpp
/// \file SteppingAction.cc
/// \brief Implementation of the SteppingAction class

#include "SteppingAction.hh"

#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4VProcess.hh"

void SteppingAction::UserSteppingAction(const G4Step *step)
{
  const G4VProcess *process = step->GetPostStepPoint()->GetProcessDefinedStep();
  if (process == nullptr)
    return;

  const G4String &processName = process->GetProcessName();
  if (processName.find("G4DNA") == G4String::npos)
    return;

  fInteractionCounter.Record(processName);
}
```

In `src/ActionInitialization.cc`: add `#include "SteppingAction.hh"` to the includes, and in `Build()` (after the existing `SetUserAction(new TrackingAction());` line — worker/serial thread only, not `BuildForMaster()`) add:

```cpp
  SetUserAction(new SteppingAction());
```

In `header/Run.hh`: add `#include "PhysicsInteractionCounter.hh"`, add this public getter after the existing `ReactionCounter *GetReactionCounter() const { return fReactionCounter; }` line:

```cpp
    PhysicsInteractionCounter *GetInteractionCounter() const { return fInteractionCounter; }
```

and add these two private members after the existing `ReactionCounter fOwnedReactionCounter;` line:

```cpp
    PhysicsInteractionCounter *fInteractionCounter;
    PhysicsInteractionCounter fOwnedInteractionCounter;
```

In `src/Run.cc`: add `#include "SteppingAction.hh"`. In the constructor, add `fInteractionCounter(nullptr)` to the initializer list, and after the existing `fReactionCounter = (timeStepAction != nullptr) ? &timeStepAction->GetReactionCounter() : &fOwnedReactionCounter;` line, add:

```cpp
    auto *steppingAction = const_cast<SteppingAction *>(
        dynamic_cast<const SteppingAction *>(G4RunManager::GetRunManager()->GetUserSteppingAction()));
    fInteractionCounter =
        (steppingAction != nullptr) ? &steppingAction->GetInteractionCounter() : &fOwnedInteractionCounter;
```

In `Run::Merge`, after the existing `fReactionCounter->Merge(*localRun->fReactionCounter); localRun->fReactionCounter->Clear();` lines, add:

```cpp
    fInteractionCounter->Merge(*localRun->fInteractionCounter);
    localRun->fInteractionCounter->Clear();
```

In `src/RunAction.cc`, after Ticket 02's energy-deposit block (still inside `if (IsMaster())`), add:

```cpp
        // Write the physical-stage interaction firing counts (merged across
        // worker threads by Run::Merge -> PhysicsInteractionCounter::Merge).
        PhysicsInteractionCounter *interactionCounter = masterRun->GetInteractionCounter();
        if (interactionCounter != nullptr)
        {
            std::ofstream interactionsOut(OutputDir::Resolve("PhysicsInteractions.Txt"));
            interactionCounter->WriteAscii(interactionsOut);
            interactionsOut.close();

            std::ofstream interactionsCsv(OutputDir::Resolve("PhysicsInteractions.csv"));
            interactionCounter->WriteCsv(interactionsCsv);
            interactionsCsv.close();

            interactionCounter->Clear();

            DnaLogger::Print(DnaLogger::Level::Info,
                              "[RunAction] physical interaction counts written "
                              "(PhysicsInteractions.Txt / PhysicsInteractions.csv)");
        }
```
