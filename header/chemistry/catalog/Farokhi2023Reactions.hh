/// \file Farokhi2023Reactions.hh
/// \brief Farokhi2023 Chemistry: reaction-table and bulk-reaction list builders
///
/// Reproduces the base-water block of Table 2 of Farokhi et al. 2023 (FLASH
/// oxygen-depletion radiolysis network). Ground truth is
/// examples/extended/medical/dna/scavenger/Rtable_O2.txt in the Geant4 11.4.1
/// source tree (type_N labels, k values and species names), not the paper
/// PDF, which has OCR artifacts.
///
/// Self-contained, project-agnostic unit: no DnaChemistryWorld, DnaLogger, or
/// other dnachem-min-specific includes -- only standard Geant4/Geant4-DNA
/// headers. Copy-paste portable to another Geant4-DNA project.
///
/// No acid-base buffer: the paper's network has no bulk H3Op(B)/OHm(B)
/// equilibria, so /chem/env/pH has no chemical effect under this Chemistry.
/// O2 is never a tracked/reaction-table species here -- unlike BoscoloChem,
/// the network never produces O2 from tracked radiolytic species; O2 only
/// ever appears as the dissolved/bulk scavenger set by /chem/env/scavenger,
/// via the three bulk reactions below (Rtable_O2.txt's "Reactions with the
/// scavengers" section). Om, O2m, O3m, HO2, HO2m stay in the shared molecule
/// set but take part only in the bulk reactions listed, nothing in the
/// reaction table. Needs ChemistryTypes.hh -- copy both files to port this
/// unit.
///
/// Reaction types (IRT_syn): Rtable_O2.txt's type_1/type_3 pairs are fully
/// diffusion-controlled (type 0, the default); its type_2/type_4 pairs are
/// partially diffusion-controlled, SetReactionType(1) unless
/// ChemistryTypes::PartialReactionsEnabled() is false (SBS: stays type 0),
/// the same mapping PureWaterReactions.cc/BoscoloChemReactions.cc already use
/// for Pimblott/LaVerne types.

#ifndef Farokhi2023Reactions_h
#define Farokhi2023Reactions_h 1

#include "chemistry/ChemistryTypes.hh"

class G4DNAMolecularReactionTable;

namespace Farokhi2023Reactions
{
  /// Populates reactionTable with the 9 base reactions of Rtable_O2.txt's
  /// "Reactions between radio-induced species" section. Looks up species
  /// configurations internally via
  /// G4MoleculeTable::Instance()->GetConfiguration(name); raises a fatal
  /// G4Exception if any expected species is missing. Call after
  /// G4ChemDissociationChannels_option1::ConstructMolecule().
  void BuildFarokhiReactions(G4DNAMolecularReactionTable* reactionTable);

  /// The dissolved-O2 bulk reactions from Rtable_O2.txt's "Reactions with the
  /// scavengers" section: e_aq + O2 -> O2-, H + O2 -> HO2., O- + O2 -> O3-
  /// (inert at 0 concentration). No acid-base buffer.
  /// Species are named as stored by G4ChemDissociationChannels_option1; the
  /// driver resolves them.
  ChemistryTypes::BulkReactionList BuildFarokhiBulkReactions();
}  // namespace Farokhi2023Reactions

#endif  // Farokhi2023Reactions_h
