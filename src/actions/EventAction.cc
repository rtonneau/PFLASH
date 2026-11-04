/// Stages pre-chemical event output and commits completed track-length rows.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#include "actions/EventAction.hh"
#include "actions/SteppingAction.hh"
#include "actions/TrackingAction.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/PreChemicalFiles.hh"

#include "G4DNAChemistryManager.hh"
#include "G4PhysChemIO.hh"
#include "G4RunManager.hh"
#include "G4Run.hh"

#include <memory>

namespace
{
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
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
    {
        // One pre-chemical file per event, staged until the next dump (ADR 0005
        // addendum). A fresh writer per event: FormattedText::CloseFile() is a
        // no-op until the first record, so a reused writer could keep a stale
        // stream open.
        const G4String outputDirectory = OutputDir::GetDirectory();
        std::string error;
        if (!PreChemicalFiles::EnsureStagingDir(outputDirectory, error))
        {
            G4Exception("EventAction::BeginOfEventAction", "PreChemicalStagingDir",
                        FatalException, error.c_str());
        }
        const G4int runId = G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID();
        const std::string fileName = PreChemicalFiles::StagingDir(outputDirectory) + "/" +
                                     PreChemicalFiles::StagedFileName(runId, eventId);

        auto *chemistryManager = G4DNAChemistryManager::Instance();
        chemistryManager->SetPhysChemIO(std::make_unique<G4PhysChemIO::FormattedText>());
        chemistryManager->WriteInto(fileName);
        DnaLogger::Print(DnaLogger::Level::Debug,
                         eventPrefix + ": pre-chemical file " + fileName);

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
        // Destroying the writer closes the event's file, even when the
        // chemistry stage never ran for this event.
        G4DNAChemistryManager::Instance()->SetPhysChemIO(nullptr);
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
}
