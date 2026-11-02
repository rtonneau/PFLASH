/// \file ReactionTableDump.hh
/// Formats and exports the live Chemistry reaction networks.
/// Dependencies: Geant4, DnaLogger and ScavengerReactionAccess; not standard-library-only.
/// Thread safety: Freeze tables before reads; callers own streams; LabelFor initializes its cache once.
///
/// Dumps the two reaction networks the chemistry stage builds to a plain
/// text file, read directly from the live Geant4 objects at call time:
/// the shared G4DNAMolecularReactionTable singleton (tracked-pair
/// and mesoscopic bulk-pair entries), and each molecule's ScavengerReactionAccess
/// process instance (per-molecule bulk-reaction network: acid-base buffer +
/// scavengers).
/// The label cache is process-wide; the reaction table must remain frozen
/// after its first use. File output also uses the project logger.

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
/// identical labels. Empty products become "(no products)". No validation;
/// string allocation exceptions propagate.
G4String FormatReactionLabel(const G4String& reactant1, const G4String& reactant2,
                              const std::vector<G4String>& productNames);

/// Writes all shared reaction-table entries, including installed bulk pairs.
/// Requires a built, frozen table. Rates are displayed in M^-1 s^-1 even for
/// first-order entries; this existing formatting limitation is not corrected here.
/// Caller owns out; failures follow its stream state/exception policy.
void WriteBimolecular(std::ostream& out);

/// Writes registered per-molecule scavenger processes after process construction.
/// Caller owns out; same rate-format limitation and stream failure policy as above.
void WriteBulkReactions(std::ostream& out);

/// Truncates filename and writes both networks after Chemistry initialization.
/// Open failure raises FatalException; subsequent write errors are not checked.
void DumpReactionTable(const G4String& filename);

/// Returns the formatted label for the bimolecular reaction with this
/// `reactionID` (G4DNAMolecularReactionData::GetReactionID(), 1-based,
/// stable once the reaction table is built). Builds and caches the full
/// id -> label table from the live G4DNAMolecularReactionTable singleton on
/// first call; safe to call concurrently from worker threads (the cache is
/// built once, read-only after). Used by TimeStepAction's live
/// per-occurrence reaction counting to avoid re-formatting the label on
/// every firing. Requires the final frozen table; invalid IDs throw out_of_range.
/// The returned reference lasts for the process; no cache-reset API exists.
const G4String& LabelFor(G4int reactionID);
}  // namespace ReactionTableDump

#endif  // ReactionTableDump_h
