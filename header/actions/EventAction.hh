/// Stages pre-chemical event output and commits completed track-length rows.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#ifndef EVENTACTION_HH
#define EVENTACTION_HH

#include "G4Event.hh"
#include "G4UserEventAction.hh"
#include "globals.hh"

/// Stages pre-chemical event output and commits completed track-length rows.
class EventAction : public G4UserEventAction
{
public:
    /// Creates event callbacks; requires a live run manager when invoked.
    EventAction();
    /// Destroy after callbacks and borrowed uses have finished.
    ~EventAction() override;

    /// Requires non-null event/current run. Resets the row and installs a fresh pre-chemical writer.
    /// Staging-directory failure raises FatalException; writer failures follow Geant4 I/O handling.
    void BeginOfEventAction(const G4Event *event) override;
    /// Requires non-null event/current run. Closes the writer and commits only non-aborted rows.
    /// Resets the event row afterward; no separate error return.
    void EndOfEventAction(const G4Event *event) override;
};

#endif