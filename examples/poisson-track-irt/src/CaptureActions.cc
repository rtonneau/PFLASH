#include "CaptureActions.hh"

#include "CaptureRecorder.hh"

#include "G4DNAChemistryManager.hh"
#include "G4Electron.hh"
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4Scheduler.hh"
#include "G4StackManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

namespace ptirt
{

CapturePrimaryGeneratorAction::CapturePrimaryGeneratorAction(CaptureRecorder& recorder)
  : fRecorder(recorder), fGun(std::make_unique<G4ParticleGun>(1))
{
  // Defaults; macros/capture.mac sets the same values explicitly.
  fGun->SetParticleDefinition(G4Electron::Definition());
  fGun->SetParticleEnergy(10 * keV);
  fGun->SetParticlePosition({0, 0, 0});
  fGun->SetParticleMomentumDirection({0, 0, 1});
}

CapturePrimaryGeneratorAction::~CapturePrimaryGeneratorAction() = default;

void CapturePrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  // Reseed first, so the gun itself could not draw from the previous state.
  fRecorder.BeginEvent(event->GetEventID(), *fGun);
  fGun->GeneratePrimaryVertex(event);
}

void CaptureSteppingAction::UserSteppingAction(const G4Step* step)
{
  fRecorder.AddDeposit(step->GetTotalEnergyDeposit());
}

void CaptureTrackingAction::PostUserTrackingAction(const G4Track* track)
{
  fRecorder.EndTrack(*track);
}

void CaptureStackingAction::NewStage()
{
  // Same trigger as the Geant4-DNA chem1 example.
  if (stackManager->GetNTotalTrack() == 0) G4DNAChemistryManager::Instance()->Run();
}

void CaptureEventAction::EndOfEventAction(const G4Event* event)
{
  fRecorder.EndEvent(*event);
}

void CaptureTimeStepAction::StartProcessing()
{
  fRecorder.CaptureAtChemistryStart();
}

void CaptureActionInitialization::Build() const
{
  SetUserAction(new CapturePrimaryGeneratorAction(fRecorder));
  SetUserAction(new CaptureSteppingAction(fRecorder));
  SetUserAction(new CaptureTrackingAction(fRecorder));
  SetUserAction(new CaptureStackingAction());
  SetUserAction(new CaptureEventAction(fRecorder));
  G4Scheduler::Instance()->SetUserAction(fTimeStepAction);
}

}  // namespace ptirt
