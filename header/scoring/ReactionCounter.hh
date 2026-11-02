/// \file ReactionCounter.hh
/// \brief Count reaction labels in configurable time bins.
/// Geant4 types; CSV output also uses analysis services and OutputDir.
/// Bin configuration is locked; callers coordinate shared counter mutation.
///
/// Accumulates per-time-bin firing counts of bimolecular reactions, keyed by
/// the same "A + B -> C + D" label ReactionTableDump uses for the static
/// reaction table, so the two outputs cross-reference directly. Callers resolve
/// labels and feed Record(); only the CSV writer requires analysis services.

#ifndef ReactionCounter_h
#define ReactionCounter_h 1

#include "globals.hh"

#include <iosfwd>
#include <map>
#include <vector>

class G4VAnalysisManager;

/// Mutable count table; allocations may throw. Stream writers use stream flags
/// or enabled stream exceptions, without a separate success return.
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

  /// Increment reactionLabel in BinFor(time); time uses Geant4 internal units.
  void Record(const G4String& reactionLabel, G4double time);

  /// Add other's counts; a distinct source is unchanged. Self-merge doubles counts.
  void Merge(const ReactionCounter& other);

  /// Remove all recorded counts; leave configuration unchanged.
  void Clear();

  /// Writes one "<label>    count = N" line per (bin, reaction), grouped by
  /// bin -- mirrors Species.Txt's per-time-block layout.
  void WriteAscii(std::ostream& out) const;

  /// Writes one ntuple row per (bin, reaction) via the given analysis
  /// manager, opened as its own "Reactions" file (mirrors ScoreSpecies's
  /// WriteWithAnalysisManager, kept separate to avoid ntuple-ID collisions
  /// with the species ntuple). Requires a non-null initialized manager and
  /// caller coordination of its file cycle. Geant4 reports analysis failures;
  /// this function does not return or check success.
  void WriteCsv(G4VAnalysisManager* analysisManager) const;

  using BinCounts = std::map<G4String, G4int>;
  using Counts = std::map<G4double, BinCounts>;
  /// Borrow the stored counts; callers must coordinate concurrent mutation.
  const Counts& GetCounts() const { return fCounts; }

  /// Assigns each distinct reaction label recorded so far a 0-based
  /// id, in ascending alphabetical order -- shared by WriteCsv() (which
  /// writes the id) and WriteMetadata() (which writes the id -> label
  /// mapping), so a run's two output files cross-reference by id instead of
  /// repeating the full label on every row. IDs depend on the labels present
  /// in this counter and need not match a later dump; use its metadata.
  std::map<G4String, G4int> BuildReactionIdMap() const;

  /// Writes one "<reactionId>,<reaction>" CSV line per distinct reaction
  /// (plus a header row), using the same id assignment as WriteCsv().
  void WriteMetadata(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // ReactionCounter_h
