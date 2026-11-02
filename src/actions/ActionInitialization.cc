/// Registers run, event and chemistry actions in Geant4 lifecycle order.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Shared initialization object; Build creates per-thread actions, BuildForMaster the master run action.

#include "actions/ActionInitialization.hh"
#include "actions/PrimaryGeneratorAction.hh"
#include "actions/StackingAction.hh"
#include "chemistry/TimeStepAction.hh"
#include "actions/RunAction.hh"
#include "actions/EventAction.hh"
#include "actions/TrackingAction.hh"
#include "actions/SteppingAction.hh"
#include "geometry/DetectorConstruction.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"

#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

#include "G4DNAChemistryManager.hh"
#include "G4H2O.hh"
#include "G4MoleculeCounter.hh"
#include "G4Scheduler.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ActionInitialization::ActionInitialization() : G4VUserActionInitialization() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ActionInitialization::~ActionInitialization() {}

void ActionInitialization::BuildForMaster() const
{
  SetUserAction(new RunAction());
  this->BuildMoleculeCounters();
}

void ActionInitialization::Build() const
{

  PrimaryGeneratorAction *primaryGeneratorAction = new PrimaryGeneratorAction;
  SetUserAction(primaryGeneratorAction);
  SetUserAction(new RunAction());
  SetUserAction(new EventAction());
  SetUserAction(new StackingAction());
  SetUserAction(new TrackingAction());
  SetUserAction(new SteppingAction());
  if (G4DNAChemistryManager::IsActivated())
  {
    // The chemistry action needs the detector's diffusion boundary. In Serial,
    // Build runs before the macro; CheckTimeStepModel reapplies the configured
    // end-time default at /run/initialize. Override /scheduler/endTime afterward.
    const auto *detector = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    G4Scheduler::Instance()->SetUserAction(
      new TimeStepAction(detector != nullptr ? detector->GetChemistryWorld() : nullptr));
    // Without the mesoscopic stage (/chem/meso/enable false, or SBS) the
    // particle-based stage runs to the end time itself: 1 us by default.
    const G4bool sbs = ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS;
    const G4bool mesoOn = MesoSettings::StageEnabled(MesoSettings::Current(), sbs);
    G4Scheduler::Instance()->SetEndTime(mesoOn ? 1. * s : 1. * us);
    // The bulk material includes the water/pH buffer and configured scavengers; it is
    // installed by DnaChemistryList::ConstructProcess(), once the chemistry-world
    // composition is known.

    this->BuildMoleculeCounters();
  }
}

void ActionInitialization::BuildMoleculeCounters() const
{
  G4cout << "[ActionInitialization::BuildMoleculeCounter] ### Building molecule counters..." << G4endl;
  // ScoreSpecies::EndOfEvent reads the counter once per event and normalises by that
  // event's own energy deposit, so the counter must start every event empty (as in
  // chem4-chem6). Otherwise later events can include earlier molecule counts.
  // The bias depends on the event yields and energy deposits.
  G4MoleculeCounterManager::Instance()->SetResetCountersBeforeEvent(true);  // defaults to false
  G4MoleculeCounterManager::Instance()->SetResetCountersBeforeRun(true);    // defaults to false
  {
    // Ignore solvent water; the fixed precision groups nearby chemistry times.
    auto counter = std::make_unique<G4MoleculeCounter>("BasicCounter");
    counter->IgnoreMolecule(G4H2O::Definition());
    counter->SetTimeComparer(G4MoleculeCounterTimeComparer::CreateWithFixedPrecision(10 * ps));
    G4MoleculeCounterManager::Instance()->RegisterCounter(std::move(counter));
  }
  G4cout << "[ActionInitialization::BuildMoleculeCounter] End building molecule counters." << G4endl;
}
