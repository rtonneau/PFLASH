/// \file PhysicsInteractionCounter.hh
/// \brief Per-event counts of caller-supplied physical-interaction labels.
/// Standard library and Geant4 types; no kernel services.
/// Callers coordinate access to shared mutable counters.
///
/// Self-contained, project-agnostic unit: no OutputDir, DnaLogger, or other
/// dnachem-min-specific includes -- only globals.hh (G4String), EventStat and
/// OutputTable (both portable) and the standard library. Copy-paste portable
/// to another Geant4-DNA project.
///
/// Counts occurrences of whatever label callers pass to Record() -- it has
/// no notion of "process" or "G4DNA"; callers decide what a label means and
/// whether to record it at all.

#ifndef PhysicsInteractionCounter_h
#define PhysicsInteractionCounter_h 1

#include "globals.hh"
#include "scoring/EventStat.hh"
#include "scoring/OutputTable.hh"

#include <cstdint>
#include <map>

/// Per-event counts of labels. Record() fills the current event;
/// CommitEvent() folds it into one EventStat per label and counts the event;
/// DiscardEvent() drops it (aborted events). A label seen in any committed
/// event contributes 0 to the events where it did not fire: the stats hold
/// only sum and sumSq, and the mean divides by CommittedEvents().
/// Not thread safe: one instance per thread, merged afterwards.
class PhysicsInteractionCounter
{
 public:
  /// Increment label by one in the current event; labels are not interpreted.
  void Record(const G4String& label);

  /// Fold the current event into the per-label stats, count it, start a new event.
  void CommitEvent();

  /// Drop the current event's counts without counting it.
  void DiscardEvent();

  /// Add other's committed stats and event count (its uncommitted event is
  /// ignored). Self-merge doubles the totals.
  void Merge(const PhysicsInteractionCounter& other);

  /// Remove all recorded data, committed or not.
  void Clear();

  using Stats = std::map<G4String, EventStat>;
  /// Borrow the per-label stats; callers must coordinate concurrent mutation.
  const Stats& GetStats() const { return fStats; }

  /// Number of committed events (the n of Mean/StdErr).
  std::int64_t CommittedEvents() const { return fCommittedEvents; }

  /// Table "physicsInteractions": process, nEvents, countSum, countSumSq,
  /// countMean, countStdErr; one row per label, sorted by label. nEvents is
  /// the committed-event count (zero-filled).
  OutputTable ToTable() const;

 private:
  std::map<G4String, G4long> fCurrent;
  Stats fStats;
  std::int64_t fCommittedEvents = 0;
};

#endif  // PhysicsInteractionCounter_h
