/// \file ReactionCounter.hh
/// \brief Per-event reaction-firing counts in configurable time bins.
/// Geant4 types and the portable EventStat / OutputTable; no kernel services.
/// Bin configuration is locked; callers coordinate shared counter mutation.
///
/// Counts firings of bimolecular reactions per (time bin, reaction), keyed by
/// the same "A + B -> C + D" label ReactionTableDump uses for the static
/// reaction table, so the two outputs cross-reference directly. Callers resolve
/// labels and feed Record(). Record() fills the current event; CommitEvent()
/// folds it into one EventStat per (bin, reaction) and counts the event, so a
/// (bin, reaction) that did not fire in an event contributes 0 to that event.
/// Not thread safe per instance: one instance per thread, merged afterwards.

#ifndef ReactionCounter_h
#define ReactionCounter_h 1

#include "globals.hh"
#include "scoring/EventStat.hh"
#include "scoring/OutputTable.hh"

#include <cstdint>
#include <map>
#include <vector>

/// Mutable count table; allocations may throw.
class ReactionCounter
{
 public:
  /// Returns the upper edge of the fixed time bin `time` falls into (times
  /// above the last edge clamp to the last edge). Input and result use
  /// Geant4 internal time units; this reads the mutex-protected shared table.
  static G4double BinFor(G4double time);

  /// The built-in 7-edge table used until/unless ConfigureBinEdges() is
  /// called with something else, in Geant4 internal time units.
  static std::vector<G4double> DefaultBinEdges();

  /// Replaces the shared bin-edge table used by BinFor() (sorted ascending,
  /// duplicates removed), in Geant4 internal time units. Supply finite values;
  /// there is no physical-range validation. An empty edges is a no-op. Callers
  /// on any worker thread may call this and BinFor() concurrently.
  static void ConfigureBinEdges(const std::vector<G4double>& edges);

  /// Parses "<e1> <e2> ... <eN> <unit>" -- the raw remaining-line string a
  /// G4UIcommand hands to a single trailing G4String parameter -- into edge
  /// values in internal (G4) units. Requires at least one edge plus a
  /// recognized time unit (picosecond/ps, nanosecond/ns, microsecond/us,
  /// millisecond/ms, second/s); returns false (with `error` set, `edgesOut`
  /// untouched) otherwise. Resolves the unit from a small local table
  /// instead of G4UnitDefinition::GetValueOf(), keeping this parser independent
  /// of kernel services for plain logic tests. Numeric range/positivity is not
  /// validated here; error is unchanged on success. See the observed mixed-build
  /// memory-corruption warning beside TimeUnitValue in the implementation.
  static G4bool ParseBinEdgesList(const G4String& text, std::vector<G4double>& edgesOut,
                                   G4String& error);

  /// Upper edge of the bin below `upperEdge` (the largest configured edge
  /// strictly smaller than it), or 0 for the first bin. Internal time units.
  static G4double LowerEdgeFor(G4double upperEdge);

  /// Increment reactionLabel by one in BinFor(time) of the current event;
  /// time uses Geant4 internal units.
  void Record(const G4String& reactionLabel, G4double time);

  /// Fold the current event into the per-(bin, reaction) stats, count it,
  /// start a new event.
  void CommitEvent();

  /// Drop the current event's counts without counting it.
  void DiscardEvent();

  /// Add other's committed stats and event count (its uncommitted event is
  /// ignored). Self-merge doubles the totals.
  void Merge(const ReactionCounter& other);

  /// Remove all recorded data, committed or not; leave configuration unchanged.
  void Clear();

  using BinStats = std::map<G4String, EventStat>;
  using Stats = std::map<G4double, BinStats>;
  /// Borrow the committed stats, keyed by bin upper edge (internal time units)
  /// then reaction label; callers must coordinate concurrent mutation.
  const Stats& GetStats() const { return fStats; }

  /// Number of committed events (the n of Mean/StdErr).
  std::int64_t CommittedEvents() const { return fCommittedEvents; }

  /// Assigns each distinct reaction label committed so far a 0-based id, in
  /// ascending alphabetical order -- shared by both tables of ToTables(), so
  /// they cross-reference by id instead of repeating the full label on every
  /// row. IDs depend on the labels present in this counter and need not match
  /// a later dump; use that dump's catalogue.
  std::map<G4String, G4int> BuildReactionIdMap() const;

  /// Two tables. "reactions": reactionId, binStart_ns, binEnd_ns, nEvents,
  /// countSum, countSumSq, countMean, countStdErr; one row per (bin,
  /// reaction), ordered by bin then reaction. binEnd_ns is the bin key
  /// BinFor() returns, binStart_ns the previous edge (0 for the first bin);
  /// nEvents is the committed-event count (zero-filled). "reactionCatalog":
  /// reactionId, reaction; one row per id.
  std::vector<OutputTable> ToTables() const;

 private:
  using BinCounts = std::map<G4String, G4int>;
  using Counts = std::map<G4double, BinCounts>;
  Counts fCurrent;
  Stats fStats;
  std::int64_t fCommittedEvents = 0;
};

#endif  // ReactionCounter_h
