/// \file SteppingAction.cc
/// \brief Implementation of the SteppingAction class

#include "actions/SteppingAction.hh"

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
