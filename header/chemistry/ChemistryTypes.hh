/// \file ChemistryTypes.hh
/// \brief Plain data types shared by every Chemistry file. Standard library only.
#ifndef ChemistryTypes_h
#define ChemistryTypes_h 1

#include <atomic>
#include <string>
#include <vector>

namespace ChemistryTypes
{
  /// One first-order or pseudo-first-order reaction of a tracked molecule
  /// against a bulk species: an acid-base buffer partner ("H3Op(B)",
  /// "OHm(B)", "H2O") or a scavenger ("O2"). rate is already in Geant4
  /// internal units. reactionType 0 = leave unset.
  struct BulkReaction
  {
    std::string partner;
    double rate;
    std::vector<std::string> products;
    int reactionType = 0;
  };

  /// All bulk reactions registered as one G4DNAScavengerProcess on `molecule`
  /// (Geant4 allows one such process per molecule).
  struct BulkReactionEntry
  {
    std::string molecule;
    std::vector<BulkReaction> reactions;
  };

  /// May be empty: a Chemistry without bulk reactions.
  using BulkReactionList = std::vector<BulkReactionEntry>;

  namespace Detail
  {
    inline std::atomic<bool> partialReactionsEnabled{true};
  }  // namespace Detail

  /// Process-wide switch read by every Chemistry builder at the place where it
  /// would call SetReactionType(1) (partially diffusion-controlled reaction,
  /// vdW reaction radius + activation rate). True (default): type 1 is
  /// applied as the catalogue says (IRT_syn). False: the reaction stays
  /// type 0, fully diffusion-controlled (SBS, chem1-chem6). The type has to
  /// be left unset, not reset: G4DNAMolecularReactionData::SetReactionType(1)
  /// cannot be undone. Set once, before the Chemistry builds its tables.
  inline bool PartialReactionsEnabled()
  {
    return Detail::partialReactionsEnabled.load();
  }

  inline void SetPartialReactionsEnabled(bool enabled)
  {
    Detail::partialReactionsEnabled.store(enabled);
  }
}  // namespace ChemistryTypes

#endif  // ChemistryTypes_h
