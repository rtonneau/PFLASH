/// Registers run, event and chemistry actions in Geant4 lifecycle order.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Shared initialization object; Build creates per-thread actions, BuildForMaster the master run action.

#ifndef ACTIONINITIALIZATION_HH
#define ACTIONINITIALIZATION_HH

#include "G4VUserActionInitialization.hh"

/// Registers run, event and chemistry actions in Geant4 lifecycle order.
class ActionInitialization : public G4VUserActionInitialization
{
public:
  /// Creates the initialization adapter; install it on the run manager before initialization.
  ActionInitialization();
  /// Destroy after callbacks and borrowed uses have finished.
  virtual ~ActionInitialization();

  /// Registers the master RunAction and molecule counters; called by the MT manager.
  virtual void BuildForMaster() const;
  /// Registers serial/worker actions in dependency order; requires detector and physics setup.
  virtual void Build() const;

  /// Registers this thread's solvent-excluding counter with 10 ps precision.
  /// Call during action setup; Geant4 may raise G4Exception for invalid state/registration.
  void BuildMoleculeCounters() const;
};
#endif // ACTIONINITIALIZATION_HH
