/// \file MesoSpeciesCounter.hh
/// \brief Per-event molecule counts at Mesoscopic stage record times.
/// Standard library and project-portable EventStat/OutputTable only.
/// Callers coordinate access to shared mutable counters.
///
/// Self-contained, project-agnostic unit: no Geant4 kernel, no other
/// dnachem-min-specific includes besides EventStat and OutputTable (both
/// portable). Copy-paste portable to another Geant4-DNA project (adjust the
/// include path prefix on copy). Same shape as PhysicsInteractionCounter.
///
/// Times are in ns.

#ifndef MesoSpeciesCounter_h
#define MesoSpeciesCounter_h 1

#include "scoring/EventStat.hh"
#include "scoring/OutputTable.hh"

#include <cstdint>
#include <map>
#include <string>

/// Per-event counts at (time, species). Add() fills the current event;
/// CommitEvent(edep_eV) folds it into one count EventStat and one G EventStat
/// per (time, species) and counts the event; DiscardEvent() drops it (aborted
/// events). A (time, species) seen in any committed event contributes 0 to
/// the events where it did not appear: the stats hold only sum and sumSq and
/// the mean divides by CommittedEvents().
/// G = count / edep_eV * 100 (molecules/100 eV). An event with edep 0 still
/// adds its counts but contributes 0 to G (no division by zero).
/// Not thread safe: one instance per thread, merged afterwards.
class MesoSpeciesCounter
{
 public:
  /// Add count molecules of species at time_ns (ns) to the current event;
  /// values are not validated.
  void Add(double time_ns, const std::string& species, long count);

  /// Fold the current event (energy deposit edep_eV, in eV) into the stats,
  /// count it, start a new event.
  void CommitEvent(double edep_eV);

  /// Drop the current event's counts without counting it.
  void DiscardEvent();

  /// Add other's committed stats and event count (its uncommitted event is
  /// ignored). Self-merge doubles the totals.
  void Merge(const MesoSpeciesCounter& other);

  /// Remove all recorded data, committed or not.
  void Clear();

  /// True when no committed (time, species) stats exist.
  bool Empty() const;

  /// Count and G statistics of one (time, species).
  struct Stat
  {
    EventStat count;
    EventStat g;
  };
  using Stats = std::map<double, std::map<std::string, Stat>>;
  /// Borrow the committed stats; callers must coordinate concurrent mutation.
  const Stats& GetStats() const { return fStats; }

  /// Number of committed events (the n of Mean/StdErr).
  std::int64_t CommittedEvents() const { return fCommittedEvents; }

  /// Table "speciesMeso" with SpeciesSchema's columns: one row per
  /// (time, species), times ascending then species sorted; nEvents is the
  /// committed-event count (zero-filled).
  OutputTable ToTable() const;

 private:
  std::map<double, std::map<std::string, long>> fCurrent;
  Stats fStats;
  std::int64_t fCommittedEvents = 0;
};

#endif  // MesoSpeciesCounter_h
