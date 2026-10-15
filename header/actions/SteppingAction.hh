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
#include "scoring/PhysicsInteractionCounter.hh"

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
