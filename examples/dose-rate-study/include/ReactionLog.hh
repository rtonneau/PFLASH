// ReactionLog: every two-molecule reaction of a shared IRT_syn history, with
// the source (track) each reactant came from. Geant4-free: the caller passes
// track IDs, parent IDs and names; ActivationObserver feeds it.
//
// Origin of a molecule. Every molecule belongs to one source, the arrival whose
// track produced it:
//   - an injected molecule (positive ID inside a source's ID range,
//     SourceInjector) belongs to that source;
//   - a molecule made during the history (negative ID from G4ITTrackHolder)
//     inherits the origin of what made it:
//       * a reaction product: Geant4 11.4.3 pushes the products before
//         G4ITModelProcessor calls UserReactionAction, so their IDs are known
//         there and RecordReaction stores their origin. Both reactants from one
//         source: that source. From two sources: the later source (largest
//         index), flagged mixed;
//       * a dissociation product: a G4ITStepProcessor secondary whose G4Track
//         parent ID is the molecule it came from (G4ITStepProcessor2.cc,
//         DealWithSecondaries); resolved through that parent;
//       * a bulk-reaction (G4DNAScavengerProcess) product: pushed straight to
//         G4ITTrackHolder, parent ID 0 (G4DNAScavengerProcess.cc, PostStepDoIt),
//         built at its parent's position. ObserveStep(), called after every time
//         step with the live molecules, gives it the origin of the nearest
//         molecule that vanished in that step without a pair reaction (its
//         parent; IRT_syn steps carry about one reaction each). Each such
//         vanished molecule is also recorded as a reaction with the partner
//         "bulk" (source kBulkSource) and the matched molecules as products.
//     Anything else leaves the molecule unknown (source -1); UnknownCount()
//     counts unknown reactants.
//
// Bulk reactions (G4DNAScavengerProcess, e.g. the acid-base buffer at pH 7)
// never reach UserReactionAction; they are recorded from ObserveStep, at the
// time of the step that ends after them.
#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace drs
{

constexpr long long kBulkSource = -2;  // partner of a bulk (first-order) reaction

struct MoleculeOrigin
{
  long long source = -1;  // source index (InjectedSource::sourceIndex); -1 = unknown
  bool mixed = false;     // made by a reaction between molecules of two sources
};

struct ReactionRecord
{
  double timeNs = 0;
  std::string reactantA;
  std::string reactantB;
  MoleculeOrigin originA;
  MoleculeOrigin originB;
  std::string products;  // names joined by " + "; empty = no product
};

class ReactionLog
{
 public:
  struct SourceRange
  {
    long long sourceIndex = 0;
    int firstTrackId = 0;  // contiguous, inclusive
    int lastTrackId = 0;
  };

  // Ranges of injected track IDs; throws std::invalid_argument when they are
  // empty, reversed or overlapping. Clears everything recorded before.
  void Configure(std::vector<SourceRange> ranges);

  // Origin of a molecule (trackId, its G4Track parent ID); cached when known.
  MoleculeOrigin Resolve(int trackId, int parentId);
  // Caches the origin of a live molecule if not yet known.
  void Learn(int trackId, int parentId) { static_cast<void>(Resolve(trackId, parentId)); }

  struct ObservedMolecule
  {
    int trackId = 0;
    int parentId = 0;
    double x = 0, y = 0, z = 0;
    const std::string* name = nullptr;  // must outlive the log (molecule configuration name)
  };
  // After a time step: every live molecule. Molecules new since the previous
  // call with no known origin take the origin of the nearest molecule that
  // vanished since then without a recorded pair reaction.
  // Each such vanished molecule is recorded as a bulk reaction at timeNs.
  void ObserveStep(double timeNs, const std::vector<ObservedMolecule>& live);
  // Molecules whose origin ObserveStep assigned by position.
  std::size_t PositionMatchedCount() const { return fPositionMatched; }
  // Bulk reactions recorded by ObserveStep (included in Records()).
  std::size_t BulkCount() const { return fBulkRecorded; }

  // One reaction: reactant IDs, parent IDs and names; product IDs and names.
  void RecordReaction(double timeNs, int idA, int parentA, const std::string& nameA, int idB, int parentB,
                      const std::string& nameB, const std::vector<int>& productIds,
                      const std::vector<std::string>& productNames);

  const std::vector<ReactionRecord>& Records() const { return fRecords; }
  // Reactants whose origin is unknown.
  std::size_t UnknownCount() const { return fUnknown; }
  // Reactions between two known sources that differ.
  std::size_t InterSourceCount() const { return fInterSource; }

 private:
  MoleculeOrigin FromRange(int trackId) const;

  std::vector<SourceRange> fRanges;  // sorted by firstTrackId
  std::unordered_map<int, MoleculeOrigin> fCache;
  std::vector<ReactionRecord> fRecords;
  struct Seen
  {
    MoleculeOrigin origin;
    double x = 0, y = 0, z = 0;
    const std::string* name = nullptr;
  };
  std::unordered_map<int, Seen> fPrevious;  // live at the previous ObserveStep
  std::unordered_set<int> fConsumed;        // pair-reaction reactants since then
  std::size_t fPositionMatched = 0;
  std::size_t fBulkRecorded = 0;
  std::size_t fUnknown = 0;
  std::size_t fInterSource = 0;
};

// Header: timeNs,reactantA,sourceA,mixedA,reactantB,sourceB,mixedB,products
void WriteReactionsCsv(std::ostream& out, const std::vector<ReactionRecord>& records);

}  // namespace drs
