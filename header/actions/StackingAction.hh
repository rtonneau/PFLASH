/// Starts chemistry after the physical track stacks are empty.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.


#ifndef G4ITStackingAction_h
#define G4ITStackingAction_h 1

#include "G4UserStackingAction.hh"
#include "globals.hh"

#include "physics/PhysicsList.hh"

/// Starts chemistry after the physical track stacks are empty.
class StackingAction : public G4UserStackingAction
{
public:
  /// Borrows the installed PhysicsList; requires that project physics-list subtype.
  StackingAction();
  /// Destroy after callbacks and borrowed uses have finished.
  virtual ~StackingAction() { ; }
  /// Runs chemistry when enabled and no physical tracks remain; requires valid physics/stack managers.
  /// Called by Geant4 when the urgent stack empties; chemistry exceptions propagate.
  virtual void NewStage();

private:
  const PhysicsList *physicsList;
};

#endif
