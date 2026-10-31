/// Counts steps selected by the G4DNA process-name filter.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "scoring/PhysicsInteractionCounter.hh"

class G4Step;

/// Counts steps selected by the G4DNA process-name filter.
class SteppingAction : public G4UserSteppingAction
{
public:
  /// Starts with an empty per-run interaction counter.
  SteppingAction() = default;
  /// Destroy after callbacks and borrowed uses have finished.
  ~SteppingAction() override = default;

  /// Requires a non-null step/post-step point. Counts its defining process only when the name
  /// contains G4DNA; null or other process names are ignored. This is not a count of all EM processes.
  void UserSteppingAction(const G4Step *step) override;

  /// Borrowed counter for this action lifetime; caller owns run resets. No failure path.
  PhysicsInteractionCounter &GetInteractionCounter() { return fInteractionCounter; }

private:
  PhysicsInteractionCounter fInteractionCounter;
};

#endif  // SteppingAction_h
