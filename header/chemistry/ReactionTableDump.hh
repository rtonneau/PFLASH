/// \file ReactionTableDump.hh
/// \brief Definition of the ReactionTableDump utility functions
///
/// Dumps the two reaction networks the chemistry stage builds to a plain
/// text file, read directly from the live Geant4 objects at call time:
/// the shared G4DNAMolecularReactionTable singleton (bimolecular pure
/// water + O2 network), and each molecule's ScavengerReactionAccess
/// process instance (per-molecule bulk-reaction network: acid-base buffer +
/// scavengers).
/// Portable utility, no project-specific state -- copy-paste portable
/// like PureWaterReactions.cc.

#ifndef ReactionTableDump_h
#define ReactionTableDump_h 1

#include "globals.hh"

#include <iosfwd>
#include <vector>

namespace ReactionTableDump
{
/// Formats "A + B -> C + D" (no rate) from reactant/product names. Pure
/// string logic, shared by WriteLine here and by TimeStepAction's live
/// per-occurrence reaction counting, so both report reactions under
/// identical labels.
G4String FormatReactionLabel(const G4String& reactant1, const G4String& reactant2,
                              const std::vector<G4String>& productNames);

/// Writes the shared bimolecular reaction table (pure water + O2 network).
void WriteBimolecular(std::ostream& out);

/// Writes the per-molecule bulk-reaction network (acid-base buffer + scavengers).
void WriteBulkReactions(std::ostream& out);

/// Opens `filename` and writes both networks, in labeled sections.
void DumpReactionTable(const G4String& filename);

/// Returns the formatted label for the bimolecular reaction with this
/// `reactionID` (G4DNAMolecularReactionData::GetReactionID(), 1-based,
/// stable once the reaction table is built). Builds and caches the full
/// id -> label table from the live G4DNAMolecularReactionTable singleton on
/// first call; safe to call concurrently from worker threads (the cache is
/// built once, read-only after). Used by TimeStepAction's live
/// per-occurrence reaction counting to avoid re-formatting the label on
/// every firing.
const G4String& LabelFor(G4int reactionID);
}  // namespace ReactionTableDump

#endif  // ReactionTableDump_h
