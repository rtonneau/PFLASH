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
  TrackingAction();
  virtual ~TrackingAction() {}
  virtual void PostUserTrackingAction(const G4Track *);
  virtual void PreUserTrackingAction(const G4Track *);

  /// Adds the current event's row (with these run and event IDs) to the
  /// live table. Does not reset the row.
  void CommitEvent(int runId, int eventId);

  /// Clears the current event's row.
  void ResetEvent();

  TrackLengthTable &GetTrackLengthTable() { return fTrackLengthTable; }

private:
  TrackLengthTable fTrackLengthTable;
  TrackLengthTable::Row fEventRow;
};

#endif
