/// Collects primary and secondary-electron track lengths for each event.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#ifndef CHEM4_TRACKINGACTION_HH
#define CHEM4_TRACKINGACTION_HH

#include "scoring/TrackLengthTable.hh"

#include <G4UserTrackingAction.hh>

/// Collects the per-event primary and secondary e- track lengths.
///
/// PostUserTrackingAction fills the current event's TrackLengthTable::Row:
/// track 1 (the gun primary) gives its length, initial and final kinetic
/// energy and end reason; every other e- track adds its length and +1 to
/// the all-generations totals, and to the first-generation totals when its
/// parent is track 1. Non-e- secondaries are ignored. EventAction commits the
/// row into the live table at the end of each non-aborted event
/// (CommitEvent) and resets it (ResetEvent). The live table persists across
/// runs; Run points at it and RunAction::BeginOfRunAction clears it.
class TrackingAction : public G4UserTrackingAction
{
public:
  /// Starts with an empty per-event row and persistent track-length table.
  TrackingAction();
  /// Destroy after callbacks and borrowed uses have finished.
  virtual ~TrackingAction() {}
  /// Requires a non-null completed track. Adds lengths in nm and primary energies in keV.
  /// Track 1 is the recorded primary; other non-electron tracks are ignored.
  virtual void PostUserTrackingAction(const G4Track *);
  /// No-op callback; the track argument is unused and may be null.
  virtual void PreUserTrackingAction(const G4Track *);

  /// Adds the current event's row (with these run and event IDs) to the
  /// live table. IDs are copied without validation; allocation exceptions propagate.
  /// Does not reset the row; caller must invoke once per accepted event.
  void CommitEvent(int runId, int eventId);

  /// Clears the current event's row, preserving the accumulated table. No failure path.
  void ResetEvent();

  /// Borrowed table for this action lifetime; owning-thread mutation only. No failure path.
  TrackLengthTable &GetTrackLengthTable() { return fTrackLengthTable; }

private:
  TrackLengthTable fTrackLengthTable;
  TrackLengthTable::Row fEventRow;
};

#endif
