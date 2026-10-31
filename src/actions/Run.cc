/// Collects event energy deposits and merges worker scoring results.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Thread-local accumulation; Geant4 serializes worker merges into the master run.

#include "actions/Run.hh"

#include "actions/RunAction.hh"
#include "scoring/ScoreSpecies.hh"
#include "chemistry/TimeStepAction.hh"
#include "actions/SteppingAction.hh"
#include "actions/TrackingAction.hh"

#include "G4Event.hh"
#include "G4HCofThisEvent.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4THitsMap.hh"
#include "G4VSensitiveDetector.hh"
#include "G4MultiFunctionalDetector.hh"
#include "Randomize.hh"

#include <map>

Run::Run() : G4Run(), fEnergyDeposit_MeV(0), fScorerRun(0), fReactionCounter(nullptr),
             fInteractionCounter(nullptr), fMesoSpeciesCounter(nullptr), fTrackLengthTable(nullptr)
{
    G4MultiFunctionalDetector *multiFunctionalDetector = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    G4int speciesCollectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");

    fScorerRun = multiFunctionalDetector->GetPrimitive(speciesCollectionId);

    auto *timeStepAction =
        dynamic_cast<TimeStepAction *>(G4Scheduler::Instance()->GetUserTimeStepAction());
    fReactionCounter =
        (timeStepAction != nullptr) ? &timeStepAction->GetReactionCounter() : &fOwnedReactionCounter;
    fMesoSpeciesCounter = (timeStepAction != nullptr) ? &timeStepAction->GetMesoSpeciesCounter()
                                                      : &fOwnedMesoSpeciesCounter;

    auto *steppingAction = const_cast<SteppingAction *>(
        dynamic_cast<const SteppingAction *>(G4RunManager::GetRunManager()->GetUserSteppingAction()));
    fInteractionCounter =
        (steppingAction != nullptr) ? &steppingAction->GetInteractionCounter() : &fOwnedInteractionCounter;

    auto *trackingAction = const_cast<TrackingAction *>(
        dynamic_cast<const TrackingAction *>(G4RunManager::GetRunManager()->GetUserTrackingAction()));
    fTrackLengthTable =
        (trackingAction != nullptr) ? &trackingAction->GetTrackLengthTable() : &fOwnedTrackLengthTable;

    fSeed = G4Random::getTheEngine()->getSeed();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

Run::~Run() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void Run::RecordEvent(const G4Event *event)
{
    if (event->IsAborted())
        return;

    G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
    G4HCofThisEvent *hitsCollections = event->GetHCofThisEvent();
    if (!hitsCollections)
        return;
    G4THitsMap<G4double> *eventEnergyDeposits =
        static_cast<G4THitsMap<G4double> *>(hitsCollections->GetHC(collectionId));

    std::map<G4int, G4double *>::iterator hit;
    for (hit = eventEnergyDeposits->GetMap()->begin(); hit != eventEnergyDeposits->GetMap()->end(); hit++)
    {
        G4double energyDeposit_MeV = *(hit->second);
        fEnergyDeposit_MeV += energyDeposit_MeV;
    }

    G4Run::RecordEvent(event);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void Run::Merge(const G4Run *aRun)
{
    if (aRun == this)
    {
        return;
    }

    const Run *localRun = static_cast<const Run *>(aRun);
    fEnergyDeposit_MeV += localRun->fEnergyDeposit_MeV;

    // A worker that got no events has no beam; the first one that has one wins.
    if (localRun->fHasBeam)
        SetBeamIfUnset(localRun->fBeam);

    ScoreSpecies *masterScorer = dynamic_cast<ScoreSpecies *>(this->fScorerRun);

    ScoreSpecies *localScorer = dynamic_cast<ScoreSpecies *>(localRun->fScorerRun);

    masterScorer->AbsorbResultsFromWorkerScorer(localScorer);

    // localRun's counters are the worker's live action counters; they are
    // cleared at the worker's next RunAction::BeginOfRunAction, not here.
    fReactionCounter->Merge(*localRun->fReactionCounter);
    fInteractionCounter->Merge(*localRun->fInteractionCounter);
    fMesoSpeciesCounter->Merge(*localRun->fMesoSpeciesCounter);
    fTrackLengthTable->Merge(*localRun->fTrackLengthTable);

    G4Run::Merge(aRun);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
