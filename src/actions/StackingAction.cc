/// Starts chemistry after the physical track stacks are empty.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#include "actions/StackingAction.hh"
#include "physics/PhysicsList.hh"
#include "chemistry/ChemUtils.hh"
#include "core/DnaLogger.hh"

#include "G4RunManager.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"
#include "G4ITTrackHolder.hh"
#include "G4AnalysisManager.hh"
#include "G4RunManager.hh"
#include "G4DNAChemistryManager.hh"
#include "G4StackManager.hh"
#include "G4Scheduler.hh"
#include "G4EmParameters.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

StackingAction::StackingAction() : G4UserStackingAction()
{
  this->physicsList = dynamic_cast<const PhysicsList *>(G4RunManager::GetRunManager()->GetUserPhysicsList());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void StackingAction::NewStage()
{
  const G4Event *currentEvent = G4RunManager::GetRunManager()->GetCurrentEvent();
  const G4int eventId = currentEvent != nullptr ? currentEvent->GetEventID() : -1;
  const G4String eventPrefix = G4String("[Stacking] Event ") + std::to_string(eventId);

  if (this->physicsList->IsChemistryEnabled() && this->stackManager->GetNTotalTrack() == 0)
  {
    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": Physics stage ends");

    DnaLogger::Print(DnaLogger::Level::Debug,
                     eventPrefix + " Chemistry TimeStepModel = " +
                         ChemUtils::GetCurrentTimeStepModelName());

    DnaLogger::Print(DnaLogger::Level::Debug,
                     eventPrefix + ": G4Scheduler End time: " +
                         std::to_string(G4Scheduler::Instance()->GetEndTime()));

    DnaLogger::Print(DnaLogger::Level::Debug,
                     eventPrefix + ": G4Scheduler Nb of trackIDs: " +
                         std::to_string(G4Scheduler::Instance()->GetNTracks()));

    // Run processes chemistry synchronously before the event can finish.
    G4DNAChemistryManager::Instance()->Run();
    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": Chemistry started");
  }
  else
  {
    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": Physics stage ends, no chemistry involved");
  }
}
