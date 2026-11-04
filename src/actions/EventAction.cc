/// Stages pre-chemical event output and commits completed track-length rows.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#include "actions/EventAction.hh"
#include "actions/SteppingAction.hh"
#include "actions/TrackingAction.hh"
#include "chemistry/TimeStepAction.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/OutputFormats.hh"
#include "scoring/PreChemicalFiles.hh"
#include "scoring/PreChemicalRecorder.hh"

#include "G4DNAChemistryManager.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4Run.hh"
#include "G4Event.hh"
#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4THitsMap.hh"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
// This thread's recorder for the current event (owned by the chemistry
// manager between Begin and EndOfEventAction), or nullptr.
thread_local PreChemicalRecorder *gRecorder = nullptr;

// This thread's TrackingAction (the per-event track-length collector), or
// nullptr where none is registered.
TrackingAction *ThreadTrackingAction()
{
    return const_cast<TrackingAction *>(dynamic_cast<const TrackingAction *>(
        G4RunManager::GetRunManager()->GetUserTrackingAction()));
}

// This thread's physical-interaction counter (SteppingAction's), or nullptr
// where none is registered (same lookup as Run::Run).
PhysicsInteractionCounter *ThreadInteractionCounter()
{
    auto *steppingAction = const_cast<SteppingAction *>(dynamic_cast<const SteppingAction *>(
        G4RunManager::GetRunManager()->GetUserSteppingAction()));
    return (steppingAction != nullptr) ? &steppingAction->GetInteractionCounter() : nullptr;
}

// This thread's reaction counter (the chemistry TimeStepAction's), or nullptr
// where none is registered (same lookup as Run::Run).
ReactionCounter *ThreadReactionCounter()
{
    auto *timeStepAction =
        dynamic_cast<TimeStepAction *>(G4Scheduler::Instance()->GetUserTimeStepAction());
    return (timeStepAction != nullptr) ? &timeStepAction->GetReactionCounter() : nullptr;
}

// This thread's mesoscopic species counter (the chemistry TimeStepAction's),
// or nullptr where none is registered (same lookup as Run::Run).
MesoSpeciesCounter *ThreadMesoSpeciesCounter()
{
    auto *timeStepAction =
        dynamic_cast<TimeStepAction *>(G4Scheduler::Instance()->GetUserTimeStepAction());
    return (timeStepAction != nullptr) ? &timeStepAction->GetMesoSpeciesCounter() : nullptr;
}

// Energy deposit of the event in eV: the sum of the "mfDetector/Species" hits
// map, the same value ScoreSpecies accumulated in its fEdep for its G
// (and Run::RecordEvent sums). Valid in EndOfEventAction, after the SD
// EndOfEvent (which resets ScoreSpecies::fEdep) and before the Run records it.
double EventEnergyDeposit_eV(const G4Event *event)
{
    G4HCofThisEvent *hitsCollections = event->GetHCofThisEvent();
    if (hitsCollections == nullptr)
        return 0.;
    const G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
    auto *edepMap = dynamic_cast<G4THitsMap<G4double> *>(hitsCollections->GetHC(collectionId));
    if (edepMap == nullptr)
        return 0.;
    G4double total = 0.;
    for (const auto &hit : *edepMap->GetMap())
        total += *hit.second;
    return total / eV;
}
}  // namespace

EventAction::EventAction() = default;
EventAction::~EventAction() = default;

void EventAction::BeginOfEventAction(const G4Event *event)
{
    const G4int eventId = event->GetEventID();
    const G4String eventPrefix = G4String("[EventAction] Event ") + std::to_string(eventId);

    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": Begin of Event");
    if (auto *trackingAction = ThreadTrackingAction())
        trackingAction->ResetEvent();
    if (auto *interactions = ThreadInteractionCounter())
        interactions->DiscardEvent();
    if (auto *reactions = ThreadReactionCounter())
        reactions->DiscardEvent();
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
    {
        // Pre-chemical rows of the event, staged until the next dump (ADR 0005
        // addendum): per-event txt/csv files, and rows for the h5/root table.
        // A fresh recorder per event; none when the format is `none`.
        const std::vector<OutputFormats::Format> formats =
            OutputFormats::Effective(OutputFormats::Product::PreChemical);
        auto *chemistryManager = G4DNAChemistryManager::Instance();
        if (!formats.empty())
        {
            const G4String outputDirectory = OutputDir::GetDirectory();
            std::string error;
            if (!PreChemicalFiles::EnsureStagingDir(outputDirectory, error))
            {
                G4Exception("EventAction::BeginOfEventAction", "PreChemicalStagingDir",
                            FatalException, error.c_str());
            }
            const G4int runId = G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID();
            auto recorder = std::make_unique<PreChemicalRecorder>(runId, eventId);
            gRecorder = recorder.get();
            chemistryManager->SetPhysChemIO(std::move(recorder));
        }
        else
        {
            // `none`: the manager skips its callbacks without a writer.
            gRecorder = nullptr;
            chemistryManager->SetPhysChemIO(nullptr);
        }

        chemistryManager->BeginOfEventAction(event);
    }
}

void EventAction::EndOfEventAction(const G4Event *event)
{
    const G4int eventId = event->GetEventID();
    const G4String eventPrefix = G4String("[EventAction] Event ") + std::to_string(eventId);
    DnaLogger::Print(DnaLogger::Level::Info,
                     eventPrefix + ": End of Event");

    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
    {
        G4DNAChemistryManager::Instance()->EndOfEventAction(event);
        // Write the event's pre-chemical rows (an empty table when the event
        // made nothing), then drop the recorder.
        if (PreChemicalRecorder *recorder = gRecorder)
        {
            std::string error;
            if (!recorder->EndEvent(PreChemicalFiles::StagingDir(OutputDir::GetDirectory()),
                                    OutputFormats::Effective(OutputFormats::Product::PreChemical),
                                    error))
            {
                G4Exception("EventAction::EndOfEventAction", "PreChemicalWriteFailed", JustWarning,
                            error.c_str());
            }
        }
        G4DNAChemistryManager::Instance()->SetPhysChemIO(nullptr);
        gRecorder = nullptr;
    }

    // One track-length row per non-aborted event (Run::RecordEvent skips
    // aborted events too), then reset the per-event values.
    if (auto *trackingAction = ThreadTrackingAction())
    {
        if (!event->IsAborted())
        {
            const G4int runId = G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID();
            trackingAction->CommitEvent(runId, eventId);
        }
        trackingAction->ResetEvent();
    }

    // Same rule for the per-event physical-interaction counts.
    if (auto *interactions = ThreadInteractionCounter())
    {
        if (!event->IsAborted())
            interactions->CommitEvent();
        else
            interactions->DiscardEvent();
    }

    // And for the per-event reaction-firing counts (the chemistry stage runs
    // inside the event, so the whole event is recorded by now).
    if (auto *reactions = ThreadReactionCounter())
    {
        if (!event->IsAborted())
            reactions->CommitEvent();
        else
            reactions->DiscardEvent();
    }

    // And for the mesoscopic species counts: the mesoscopic stage runs inside
    // the event (hand-over in TimeStepAction), so they are complete here.
    if (auto *meso = ThreadMesoSpeciesCounter())
    {
        if (!event->IsAborted())
            meso->CommitEvent(EventEnergyDeposit_eV(event));
        else
            meso->DiscardEvent();
    }
}
