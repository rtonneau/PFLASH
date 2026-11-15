// CaptureActions: thin Geant4 user actions of `capture`; each forwards to the
// CaptureRecorder (see CaptureRecorder.hh for what each one records).
//
// The chemistry is started the way the Geant4-DNA chem examples do it: when the
// physics stack is empty, CaptureStackingAction::NewStage calls
// G4DNAChemistryManager::Run. CaptureTimeStepAction::StartProcessing then
// captures the precursors and stops the scheduler.
//
// Ownership: the run manager owns the actions built here; the G4Scheduler does
// NOT delete its user time-step action, so CaptureActionInitialization only
// receives a pointer to one owned by main().
#pragma once

#include <memory>

#include "G4UserEventAction.hh"
#include "G4UserStackingAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4UserTimeStepAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

class G4ParticleGun;

namespace ptirt
{

class CaptureRecorder;

class CapturePrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
 public:
  explicit CapturePrimaryGeneratorAction(CaptureRecorder& recorder);
  ~CapturePrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event* event) override;
  const G4ParticleGun& GetGun() const { return *fGun; }

 private:
  CaptureRecorder& fRecorder;
  std::unique_ptr<G4ParticleGun> fGun;  // its messenger provides /gun/...
};

class CaptureSteppingAction : public G4UserSteppingAction
{
 public:
  explicit CaptureSteppingAction(CaptureRecorder& recorder) : fRecorder(recorder) {}
  void UserSteppingAction(const G4Step* step) override;

 private:
  CaptureRecorder& fRecorder;
};

class CaptureTrackingAction : public G4UserTrackingAction
{
 public:
  explicit CaptureTrackingAction(CaptureRecorder& recorder) : fRecorder(recorder) {}
  void PostUserTrackingAction(const G4Track* track) override;

 private:
  CaptureRecorder& fRecorder;
};

class CaptureStackingAction : public G4UserStackingAction
{
 public:
  void NewStage() override;
};

class CaptureEventAction : public G4UserEventAction
{
 public:
  explicit CaptureEventAction(CaptureRecorder& recorder) : fRecorder(recorder) {}
  void EndOfEventAction(const G4Event* event) override;

 private:
  CaptureRecorder& fRecorder;
};

class CaptureTimeStepAction : public G4UserTimeStepAction
{
 public:
  explicit CaptureTimeStepAction(CaptureRecorder& recorder) : fRecorder(recorder) {}
  void StartProcessing() override;

 private:
  CaptureRecorder& fRecorder;
};

// Serial only (capture refuses the MT run manager).
class CaptureActionInitialization : public G4VUserActionInitialization
{
 public:
  CaptureActionInitialization(CaptureRecorder& recorder, CaptureTimeStepAction* timeStepAction)
    : fRecorder(recorder), fTimeStepAction(timeStepAction)
  {}
  void Build() const override;

 private:
  CaptureRecorder& fRecorder;
  CaptureTimeStepAction* fTimeStepAction;  // not owned
};

}  // namespace ptirt
