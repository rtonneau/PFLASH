/// \file PureWaterReactions.hh
/// \brief The PureWater Chemistry: portable pure-water radiolysis + O2-derived
/// reaction-table builder and its bulk-reaction list
///
/// PureWater is the default Chemistry (see ChemistryRegistry, /chem/select)
/// and the reference to copy when adding another one (BoscoloChemReactions.hh
/// started that way).
///
/// Self-contained, project-agnostic unit: no DnaChemistryWorld, DnaLogger, or
/// other dnachem-min-specific includes -- only standard Geant4/Geant4-DNA
/// headers. Copy-paste portable to another Geant4-DNA project.
///
/// Adds, directly to the supplied reaction table:
///   - the 9 base pure-water radiolysis reactions
///   - e_aq/H/O- + O2 against *tracked* radiolytic O2
///     (UHDR: ChemOxygenWaterBuilder::OxygenScavengerReaction)
///   - the O2-/HO2/HO2-/O-/O3- second-order network
///     (UHDR: ChemOxygenWaterBuilder::SecondOrderReactionExtended,
///      O2-relevant subset only -- NO2-/CO2/HCO3-/N2O/MeOH lines excluded,
///      out of project scope)
///
/// Also supplies the pH-driven acid-base buffer equilibria against the bulk
/// H3Op(B)/OHm(B)/H2O pseudo-species and the dissolved-O2 scavenger
/// reactions e_aq/H/O- + O2 (bulk) as plain data, the bulk-reaction list
/// (BuildPureWaterBulkReactions);
/// the driver turns that list into G4DNAScavengerProcess registrations
/// (particle stage) and reaction-table entries (mesoscopic stage).
/// Needs ChemistryTypes.hh -- copy both files to port this unit.
///
/// Reaction types (IRT_syn): every tracked pair that G4EmDNAChemistry_option3
/// lists as Type II or Type IV calls SetReactionType(1) (partially
/// diffusion-controlled) unless ChemistryTypes::PartialReactionsEnabled() is
/// false, which the driver sets for SBS; the others stay type 0 (fully
/// diffusion-controlled). The bulk-reaction list keeps its own types (the
/// acid-base equilibria 6/7/8). The driver also adds every bulk reaction to
/// the reaction table, for the mesoscopic stage; a bulk reaction whose pair
/// is already in the table (e.g. e_aq + O2, tracked and bulk O2 share one
/// configuration) must have the same rate and products and shares that entry.

#ifndef PureWaterReactions_h
#define PureWaterReactions_h 1

#include "chemistry/ChemistryTypes.hh"

class G4DNAMolecularReactionTable;

namespace PureWaterReactions
{
  /// Populates reactionTable with the full pure-water + O2 ordinary
  /// (non-bulk) reaction set. Looks up species configurations internally via
  /// G4MoleculeTable::Instance()->GetConfiguration(name); raises a fatal
  /// G4Exception if any expected species is missing. Call after
  /// G4ChemDissociationChannels_option1::ConstructMolecule().
  void BuildPureWaterReactions(G4DNAMolecularReactionTable* reactionTable);

  /// The pH-driven acid-base buffer equilibria against the bulk H3Op(B) /
  /// OHm(B) / H2O pseudo-species (UHDR: ChemPureWaterBuilder::
  /// WaterScavengerReaction) plus the dissolved-O2 scavenger reactions
  /// e_aq/H/O- + O2 (bulk, UHDR rates; inert at 0 concentration), as plain
  /// data. Species are named as stored by
  /// G4ChemDissociationChannels_option1; the driver resolves them.
  ChemistryTypes::BulkReactionList BuildPureWaterBulkReactions();
}  // namespace PureWaterReactions

#endif  // PureWaterReactions_h
