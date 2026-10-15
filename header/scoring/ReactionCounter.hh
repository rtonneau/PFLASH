/// \file ReactionCounter.hh
/// \brief Definition of the ReactionCounter class
///
/// Accumulates per-time-bin firing counts of bimolecular reactions, keyed by
/// the same "A + B -> C + D" label ReactionTableDump uses for the static
/// reaction table, so the two outputs cross-reference directly. Pure
/// accumulation/binning logic -- no Geant4 runtime dependency, callers
/// resolve reaction labels and feed them in via Record().

#ifndef ReactionCounter_h
#define ReactionCounter_h 1

#include "globals.hh"

#include <iosfwd>
#include <map>
#include <vector>

class G4VAnalysisManager;

class ReactionCounter
{
 public:
  /// Returns the upper edge of the fixed time bin `time` falls into (times
  /// above the last edge clamp to the last edge).
  static G4double BinFor(G4double time);

  /// The built-in 7-edge table used until/unless ConfigureBinEdges() is
  /// called with something else.
  static std::vector<G4double> DefaultBinEdges();

  /// Replaces the shared bin-edge table used by BinFor() (sorted ascending,
  /// duplicates removed). An empty `edges` is a no-op. Thread-safe: callers
  /// on any worker thread may call this and BinFor() concurrently.
  static void ConfigureBinEdges(const std::vector<G4double>& edges);

  /// Parses "<e1> <e2> ... <eN> <unit>" -- the raw remaining-line string a
  /// G4UIcommand hands to a single trailing G4String parameter -- into edge
  /// values in internal (G4) units. Requires at least one edge plus a
  /// recognized time unit (picosecond/ps, nanosecond/ns, microsecond/us,
  /// millisecond/ms, second/s); returns false (with `error` set, `edgesOut`
  /// untouched) otherwise. Resolves the unit from a small local table
  /// instead of G4UnitDefinition::GetValueOf() -- this class stays free of
  /// any live-Geant4-DLL dependency, so it's safe to exercise from a plain
  /// logic unit test.
  static G4bool ParseBinEdgesList(const G4String& text, std::vector<G4double>& edgesOut,
                                   G4String& error);

  void Record(const G4String& reactionLabel, G4double time);

  /// Adds `other`'s counts into this counter; `other` is left unchanged.
  void Merge(const ReactionCounter& other);

  void Clear();

  /// Writes one "<label>    count = N" line per (bin, reaction), grouped by
  /// bin -- mirrors Species.Txt's per-time-block layout.
  void WriteAscii(std::ostream& out) const;

  /// Writes one ntuple row per (bin, reaction) via the given analysis
  /// manager, opened as its own "Reactions" file (mirrors ScoreSpecies's
  /// WriteWithAnalysisManager, kept separate to avoid ntuple-ID collisions
  /// with the species ntuple).
  void WriteCsv(G4VAnalysisManager* analysisManager) const;

  using BinCounts = std::map<G4String, G4int>;
  using Counts = std::map<G4double, BinCounts>;
  const Counts& GetCounts() const { return fCounts; }

  /// Assigns each distinct reaction label recorded so far a stable 0-based
  /// id, in ascending alphabetical order -- shared by WriteCsv() (which
  /// writes the id) and WriteMetadata() (which writes the id -> label
  /// mapping), so a run's two output files cross-reference by id instead of
  /// repeating the full label on every row.
  std::map<G4String, G4int> BuildReactionIdMap() const;

  /// Writes one "<reactionId>,<reaction>" CSV line per distinct reaction
  /// (plus a header row), using the same id assignment as WriteCsv().
  void WriteMetadata(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // ReactionCounter_h
