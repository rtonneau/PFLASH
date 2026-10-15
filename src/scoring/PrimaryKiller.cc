#include "scoring/PrimaryKiller.hh"

#include <G4Event.hh>
#include <G4RunManager.hh>
#include <G4SystemOfUnits.hh>
#include <G4UIcmdWith3VectorAndUnit.hh>
#include <G4UIcmdWithADoubleAndUnit.hh>
#include <G4UIcmdWithAnInteger.hh>

#include <G4UnitsTable.hh>

namespace
{
// Set when track 1 is killed by a PrimaryKiller on this thread; reset at the
// start of each event (Initialize). Thread-local: each MT worker tracks its
// own events.
G4ThreadLocal G4bool gPrimaryKilled = false;
}  // namespace

G4bool PrimaryKiller::PrimaryKilledThisEvent()
{
  return gPrimaryKilled;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

PrimaryKiller::PrimaryKiller(G4String name, G4int depth)
    : G4VPrimitiveScorer(name, depth), G4UImessenger()
{
  fELoss = 0.; // cumulated energy for current event

  fELossRange_Min = DBL_MAX; // fELoss from which the primary is killed
  fELossRange_Max = DBL_MAX; // fELoss from which the event is aborted
  fKineticE_Min = 0;         // kinetic energy below which the primary is killed
  fPhantomSize = G4ThreeVector(1 * km, 1 * km, 1 * km);
  fVerbose = 0;

  fpELossUI = new G4UIcmdWithADoubleAndUnit("/primaryKiller/eLossMin", this);
  fpAbortEventIfELossUpperThan = new G4UIcmdWithADoubleAndUnit("/primaryKiller/eLossMax", this);
  fpMinKineticE = new G4UIcmdWithADoubleAndUnit("/primaryKiller/minKineticE", this);
  fpSizeUI = new G4UIcmdWith3VectorAndUnit("/primaryKiller/setSize", this);
  fpSizeUI->SetDefaultUnit("um");
  fpVerboseUI = new G4UIcmdWithAnInteger("/primaryKiller/verbose", this);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

PrimaryKiller::~PrimaryKiller()
{
  delete fpELossUI;
  delete fpAbortEventIfELossUpperThan;
  delete fpSizeUI;
  delete fpVerboseUI;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void PrimaryKiller::SetNewValue(G4UIcommand *command, G4String newValue)
{
  if (command == this->fpELossUI)
  {
    this->fELossRange_Min = this->fpELossUI->GetNewDoubleValue(newValue);
  }
  else if (command == this->fpAbortEventIfELossUpperThan)
  {
    this->fELossRange_Max = this->fpAbortEventIfELossUpperThan->GetNewDoubleValue(newValue);
  }
  else if (command == this->fpSizeUI)
  {
    this->fPhantomSize = this->fpSizeUI->GetNew3VectorValue(newValue);
  }
  else if (command == this->fpVerboseUI)
  {
    this->fVerbose = this->fpVerboseUI->GetNewIntValue(newValue);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4bool PrimaryKiller::ProcessHits(G4Step *aStep, G4TouchableHistory *)
{
  const G4Track *track = aStep->GetTrack();
  G4ThreeVector pos = aStep->GetPostStepPoint()->GetPosition();

  if (std::abs(pos.x()) > this->fPhantomSize.getX() / 2 || std::abs(pos.y()) > this->fPhantomSize.getY() / 2 || std::abs(pos.z()) > this->fPhantomSize.getZ() / 2)
  {
    ((G4Track *)track)->SetTrackStatus(G4TrackStatus::fStopAndKill);
    if (track->GetTrackID() == 1)
      gPrimaryKilled = true;
    return FALSE;
  }

  // Next part will be focus on secondary electrons
  if (track->GetTrackID() != 1 || track->GetParticleDefinition()->GetPDGEncoding() != 11)
    return FALSE;

  //-------------------

  double kineticE = aStep->GetPostStepPoint()->GetKineticEnergy();

  G4double eLoss = aStep->GetPreStepPoint()->GetKineticEnergy() - kineticE;

  if (eLoss == 0.)
    return FALSE;

  //-------------------

  this->fELoss += eLoss;

  if (this->fELoss > this->fELossRange_Max)
  {

    if (this->fVerbose > 0)
    {
      int eventID =
          G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();

      G4cout << " * PrimaryKiller: aborts event " << eventID << " energy loss "
                                                                "is too large. \n"
             << " * Energy loss by primary is: "
             << G4BestUnit(this->fELoss, "Energy")
             << ". Event is aborted if the Eloss is > "
             << G4BestUnit(this->fELossRange_Max, "Energy")
             << "Last energy loss is: " << G4BestUnit(eLoss, "Energy")
             << G4endl;
    }

    G4RunManager::GetRunManager()->AbortEvent();
  }

  if (this->fELoss >= this->fELossRange_Min || kineticE <= this->fKineticE_Min)
  {
    ((G4Track *)track)->SetTrackStatus(G4TrackStatus::fStopAndKill);
    gPrimaryKilled = true; // only track 1 (e-) reaches this branch
    if (this->fVerbose > 0)
    {
      G4cout << "kill track at : " << '\n'
             << G4BestUnit(kineticE, "Energy")
             << ", E loss is: "
             << G4BestUnit(this->fELoss, "Energy")
             << " /fELossMax: "
             << G4BestUnit(this->fELossRange_Max, "Energy")
             << ", EThreshold is: "
             << G4BestUnit(this->fKineticE_Min, "Energy")
             << G4endl;
    }
  }

  return TRUE;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void PrimaryKiller::Initialize(G4HCofThisEvent * /*HCE*/)
{
  // Called once per event (G4SDManager::PrepareNewEvent), before tracking.
  fELoss = 0.;
  gPrimaryKilled = false;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void PrimaryKiller::EndOfEvent(G4HCofThisEvent *) {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void PrimaryKiller::DrawAll()
{
  ;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void PrimaryKiller::PrintAll() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....
