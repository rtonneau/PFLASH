/// \file ActionInitialization.cc
/// \brief Implementation of the ActionInitialization class

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

  PrimaryGeneratorAction *primGenAction = new PrimaryGeneratorAction;
  SetUserAction(primGenAction);
  SetUserAction(new RunAction());
  SetUserAction(new EventAction());
  SetUserAction(new StackingAction());
  SetUserAction(new TrackingAction());
  SetUserAction(new SteppingAction());
  // Chemistry part
  if (G4DNAChemistryManager::IsActivated())
  {
    // G4Scheduler::Instance()->SetVerbose(1);

    // Chemistry end time: 1 s, the end of the mesoscopic stage (the UHDR example
    // runs to 1 s too). Override with /scheduler/endTime after /run/initialize.
    // The TimeStepAction hands over from the particle-based
    // stage (IRT_syn) to the mesoscopic stage, whose mesh spans the chemistry
    // world's boundary (UHDR example: ActionInitialization::Build).
    const auto *detector = dynamic_cast<const DetectorConstruction *>(
      G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    G4Scheduler::Instance()->SetUserAction(
      new TimeStepAction(detector != nullptr ? detector->GetChemistryWorld() : nullptr));
    // Without the mesoscopic stage (/chem/meso/enable false, or SBS) the
    // particle-based stage runs to the end time itself: 1 us by default.
    const G4bool sbs = ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS;
    const G4bool mesoOn = MesoSettings::StageEnabled(MesoSettings::Current(), sbs);
    G4Scheduler::Instance()->SetEndTime(mesoOn ? 1. * s : 1. * us);
    //==========================================================================
    // G4Scheduler::Instance()->SetMaxNbSteps(10);
    // You may decide to stop the simulation after N steps
    //==========================================================================

    // The bulk-scavenger material (only needed for the optional O2 scavenger) is
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
  // chem4-chem6). With false, event k would see the cumulative count of events 1..k and
  // the summed G-values would be inflated by (nEvents+1)/2.
  G4MoleculeCounterManager::Instance()->SetResetCountersBeforeEvent(true);  // defaults to false
  G4MoleculeCounterManager::Instance()->SetResetCountersBeforeRun(true);    // defaults to false
                                                                            // Basic (built-in) Counters
  {
    // Basic molecule counter using a fixed time precision.
    // this will create many records {molecule -> {time -> count}}
    auto counter = std::make_unique<G4MoleculeCounter>("BasicCounter");
    counter->IgnoreMolecule(G4H2O::Definition());
    counter->SetTimeComparer(G4MoleculeCounterTimeComparer::CreateWithFixedPrecision(10 * ps));
    G4MoleculeCounterManager::Instance()->RegisterCounter(std::move(counter));
  }
  G4cout << "[ActionInitialization::BuildMoleculeCounter] End building molecule counters." << G4endl;
}
