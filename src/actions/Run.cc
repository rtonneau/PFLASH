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

Run::Run() : G4Run(), fSumEne(0), fScorerRun(0), fReactionCounter(nullptr), fInteractionCounter(nullptr), fMesoSpeciesCounter(nullptr), fTrackLengthTable(nullptr)
{
    G4MultiFunctionalDetector *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    G4int CollectionIDspecies = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");

    fScorerRun = mfdet->GetPrimitive(CollectionIDspecies);

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

    G4int CollectionID = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
    // G4int evtNb = event->GetEventID();

    //  G4cout << G4endl << "---> end of event: " << evtNb << G4endl;
    // Hits collections
    //
    G4HCofThisEvent *HCE = event->GetHCofThisEvent();
    if (!HCE)
        return;
    G4THitsMap<G4double> *evtMap = static_cast<G4THitsMap<G4double> *>(HCE->GetHC(CollectionID));

    std::map<G4int, G4double *>::iterator itr;
    for (itr = evtMap->GetMap()->begin(); itr != evtMap->GetMap()->end(); itr++)
    {
        G4double edep = *(itr->second);
        fSumEne += edep;
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
    fSumEne += localRun->fSumEne;

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
