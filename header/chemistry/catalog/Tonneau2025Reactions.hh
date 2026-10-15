/// \file Tonneau2025Reactions.hh
/// \brief The Tonneau2025 Chemistry: Table 2 of Tonneau et al., Phys. Med.
/// Biol. 70 (2025) 235021 on tracked Geant4-DNA molecules
///
/// Built from Tonneau2025Table (the 73 rows as plain data), with the table's
/// rate constants. The paper solves the network as a homogeneous ODE from
/// 100 ns; here it acts on tracked molecules from the end of the pre-chemical
/// stage (particle-based IRT_syn stage, then the mesoscopic stage).
///
/// Self-contained, project-agnostic unit: no DnaChemistryWorld, DnaLogger, or
/// other dnachem-min-specific includes -- only standard Geant4/Geant4-DNA
/// headers. Needs ChemistryTypes.hh and Tonneau2025Table.{hh,cc}; copy them
/// together to port this unit (adjust the include path prefix on copy).
///
/// Species names: e-aq -> e_aq, H. -> H, OH. -> deg+"OH", OH- -> OHm, H3O+ ->
/// H3Op, O- -> Om, O2- -> O2m, O3- -> O3m, HO2. -> "HO2"+deg, HO2- -> HO2m,
/// HO3, O3, O2, H2, H2O2 unchanged (deg = the UTF-8 degree sign, spelled from
/// explicit bytes in the .cc, as in PureWaterReactions.cc). H2O is the solvent: dropped from reactants and products,
/// or turned into the "H2O" bulk partner of a pseudo-first-order reaction.
///
/// Extra molecule (ADR 0007): ConstructTonneau2025Molecules creates HO3 (D =
/// 2.0e-9 m2/s and vdW radius 0.20 nm, both O3 analogues, charge 0; see
/// docs/literature/ho3-parameters.md). O3 and O3m come from
/// G4ChemDissociationChannels_option1 with Geant4's own parameters.
///
/// Routing (see Placements()):
///   - Table: two tracked molecules (after dropping H2O), in the reaction
///     table. R1 is e_aq + e_aq, its two H2O dropped.
///   - TableAndBulk: in the reaction table (tracked pair) and in the
///     bulk-reaction list. The acid-base reactions with an H3O+ or OH-
///     reactant react with the tracked ion (table) and with the fixed pH
///     buffer H3Op(B)/OHm(B) (bulk), as PureWater does; R59 gives two bulk
///     entries (OHm + H3Op(B), H3Op + OHm(B)). R7, R31, R47 react with tracked
///     O2 (table) and dissolved O2 set by /chem/env/scavenger O2 (bulk, inert
///     at 0); both share one reaction-table entry (same rate and products).
///     R54 (O3- + H3O+) is routed here too: the paper's H3O+ includes the
///     pH background, which only the bulk entry represents.
///   - Bulk: first-order reactions, against the "H2O" partner (Geant4's
///     G4DNAScavengerProcess uses an "H2O" partner's rate as a first-order
///     rate): the water reactions R57, R60, R62, R64, R66, R69, R71, R73 and
///     the unimolecular decays R53 (O3- -> O2 + O-) and R55 (HO3 -> O2 + OH),
///     as PureWater does for O3- decay. In a bulk reaction an H3O+ or OH-
///     product goes to H3Op(B)/OHm(B), i.e. it is absorbed by the fixed buffer.
///   - WaterTable: R23 (H + H2O -> H2 + OH) and R71 (H + H2O -> e_aq +
///     H3O+) share the pair H + H2O. G4DNAScavengerProcess keeps one reaction
///     per (molecule, partner), so only one can be a bulk reaction. R71 (the
///     acid-base partner of R70, also in PureWater) is the bulk one; both are
///     also added straight to the reaction table, R23 first, so the
///     mesoscopic stage (which sums every listed entry) runs both and the
///     driver's shared-pair lookup finds R71 (the table keeps the last entry
///     set for a pair). Consequence: R23 is absent from the particle-based
///     stage (11 s^-1, i.e. a probability of ~6e-8 per H over the default
///     5 ns hand-over time).
///   - Buffer: R58 (2 H2O -> H3O+ + OH-, zero order) has no tracked reactant;
///     the water autoionisation equilibrium R58/R59 is what the fixed pH
///     buffer H3Op(B)/OHm(B) stands for.
///
/// Table 2 has no O(3P): the "Oxy" atoms made by water dissociation take part
/// in no reaction under this Chemistry and persist.
///
/// Not reproduced: the paper's dose-driven pH drift. The bulk pool
/// H3Op(B)/OHm(B) is fixed by /chem/env/pH for the whole run (Geant4 never
/// changes their counts), so H3O+/OH- made by bulk reactions vanish into it.
///
/// Rate constants, as in Tonneau2025Table: M^-1 s^-1 rows are entered as
/// k * 1e-3 m3/(mole s); s^-1 rows as k / s. Like-species rows (R1, R19, R24,
/// R34, R41, R46) take the table's k as Geant4's rate constant, the convention
/// PureWater and BoscoloChem use for their tabulated values.
/// Open point (not changed here): Tonneau2025Table reads R57 (0.155), R66
/// (19.0), R69 (1.27e6) and R73 (1.27e6) as s^-1. Detailed balance with their
/// reverse reactions (R56, R67, R68, R72) and the pKa values of HO2, H, H2O2,
/// OH suggests M^-1 s^-1 to be multiplied by [H2O] = 55.5 M (PureWater does
/// so for the same four reactions; G4EmDNAChemistry_option3 does not).
///
/// Reaction types (reaction table, IRT_syn): a pair also in PureWater copies
/// its type ("PW"). The other pairs ("G4") are in no Geant4 stock table
/// (G4EmDNAChemistry_option3, UHDR, scavenger, moleculardna); they follow
/// option3's classification: partially diffusion-controlled (type 1, option3
/// Type II neutral / Type IV ionic) below the diffusion limit, fully
/// diffusion-controlled (type 0, Type I / III) at or above it, where type 1
/// would give an activation rate <= 0. k_diff is computed by
/// G4DNAMolecularReactionData::SetReactionType from the vdW radii and D.
///   R1  e_aq + e_aq      0 PW       R37 HO2 + HO2m      1 G4
///   R2  e_aq + H         0 PW       R38 HO2 + O3m       1 G4
///   R3  e_aq + OH        1 PW       R39 HO2 + O3        1 G4
///   R4  e_aq + Om        1 PW       R40 O2m + Om        1 PW
///   R5  e_aq + H2O2      1 PW       R41 O2m + O2m       0 PW
///   R6  e_aq + HO2m      1 PW       R42 O2m + H2O2      1 G4
///   R7  e_aq + O2        1 PW (+ bulk)   R43 O2m + HO2m  1 G4
///   R8  e_aq + O2m       1 PW       R44 O2m + O3m       1 G4
///   R9  e_aq + HO2       1 PW       R45 O2m + O3        1 G4
///   R10 e_aq + O3m       1 G4       R46 Om + Om         1 PW
///   R11 e_aq + O3        1 G4 (k = 0.98 k_diff)   R47 Om + O2  1 PW (+ bulk)
///   R12 OH + H2O2        1 PW       R48 Om + H2         1 PW
///   R13 OH + Om          0 PW pair is 1, see below   R49 Om + H2O2  1 PW
///   R14 OH + HO2m        1 PW       R50 Om + HO2m       1 PW
///   R15 OH + O3m         1 PW       R51 Om + O3m        1 PW
///   R16 OH + O3m         1 PW       R52 Om + O3         1 G4
///   R17 OH + O3          1 G4       R53 O3m (decay)     bulk vs H2O
///   R18 OH + H           1 PW       R54 O3m + H3Op      0 PW (+ bulk)
///   R19 OH + OH          1 PW       R55 HO3 (decay)     bulk vs H2O
///   R20 OH + H2          1 PW       R56 HO2 + OHm       1 PW (+ bulk)
///   R21 OH + HO2         1 PW       R57 O2m + H2O       bulk
///   R22 OH + O2m         1 PW       R58 2 H2O           buffer
///   R23 H + H2O          table vs H2O, 0   R59 H3Op + OHm  0 PW (+ 2 bulk)
///   R24 H + H            0 PW       R60 OH + H2O        bulk
///   R25 H + H2O2         1 PW       R61 H3Op + Om       1 PW (+ bulk)
///   R26 H + Om           0 PW       R62 H2O2 + H2O      bulk
///   R27 H + HO2m         1 G4       R63 H3Op + HO2m     1 PW (+ bulk)
///   R28 H + O3m          1 G4       R64 HO2 + H2O       bulk, type 6 PW
///   R29 H + O2m          1 PW       R65 H3Op + O2m      1 PW (+ bulk, type 6 PW)
///   R30 H + O3           0 G4 (k = 1.4 k_diff)   R66 e_aq + H2O  bulk
///   R31 H + O2           1 PW (+ bulk)   R67 H + OHm     1 PW (+ bulk)
///   R32 H + HO2          1 PW       R68 H2O2 + OHm      1 PW (+ bulk, type 7 PW)
///   R33 HO2 + O2m        1 PW       R69 HO2m + H2O      bulk, type 7 PW
///   R34 HO2 + HO2        1 PW       R70 e_aq + H3Op     1 PW (+ bulk)
///   R35 HO2 + Om         1 G4       R71 H + H2O         bulk + table vs H2O, 0
///   R36 HO2 + H2O2       1 G4       R72 OH + OHm        1 PW (+ bulk, type 8 PW)
///                                   R73 Om + H2O        bulk, type 8 PW
/// Bulk entries take PureWater's type for the same (molecule, partner) pair
/// (the G4ChemEquilibrium types 6/7/8 above), 0 otherwise.
/// Deviation: R13 (OH + O-) is type 1 in PureWater, but Table 2's k = 2.5e10
/// is 1.7 times the pair's k_diff, so type 1 would give an activation rate
/// <= 0 (the driver warns about it); it is type 0 here.
/// Same pair twice: R15 and R16 (OH + O3-, two product channels). The
/// mesoscopic stage runs both. The particle-based stage resolves a pair
/// through the last entry set (R16) and visits the partner once per listed
/// entry, so there R16 stands for both channels at about twice its rate.
///
/// BuildTonneau2025Reactions checks that both reactants of every type-1
/// reaction have a vdW radius > 0 (fatal G4Exception otherwise).

#ifndef Tonneau2025Reactions_h
#define Tonneau2025Reactions_h 1

#include "chemistry/ChemistryTypes.hh"

#include <string>
#include <vector>

class G4DNAMolecularReactionTable;

namespace Tonneau2025Reactions
{
  /// Populates reactionTable with every Table 2 row routed to the reaction
  /// table (Table, TableAndBulk, WaterTable). Looks up species configurations
  /// via G4MoleculeTable::Instance()->GetConfiguration(name); raises a fatal
  /// G4Exception if any is missing, or if a type-1 reactant has no vdW
  /// radius. Call after G4ChemDissociationChannels_option1::ConstructMolecule()
  /// and ConstructTonneau2025Molecules().
  void BuildTonneau2025Reactions(G4DNAMolecularReactionTable* reactionTable);

  /// The bulk-reaction list: the acid-base block against H3Op(B)/OHm(B)/H2O
  /// (R54, R56, R57, R59 to R73 without R58), the first-order decays R53, R55
  /// (against H2O) and the dissolved-O2 reactions R7, R31, R47 (inert while
  /// /chem/env/scavenger O2 is 0), as plain data. Species are named as stored
  /// by G4ChemDissociationChannels_option1; the driver resolves them.
  ChemistryTypes::BulkReactionList BuildTonneau2025BulkReactions();

  /// Creates the HO3 molecule definition and configuration (master thread,
  /// from DnaChemistryList::ConstructMolecule). Idempotent: a second call
  /// changes nothing.
  void ConstructTonneau2025Molecules();

  // ---- Geant4-free description of the routing, for tests ----

  /// Where a Table 2 row goes (see the file comment).
  enum class Route
  {
    Table,
    TableAndBulk,
    Bulk,
    WaterTable,          ///< R23: reaction table only, against "H2O"
    BulkAndWaterTable,   ///< R71: bulk list, and reaction table against "H2O"
    Buffer               ///< R58: no reaction, held by the fixed pH buffer
  };

  struct Placement
  {
    int id;          ///< Table 2 row, 1..73
    Route route;
    int tableType;   ///< reaction-table type (0 or 1); -1 when not in the table
    int bulkType;    ///< bulk reaction type (0, 6, 7, 8); -1 when not in the bulk list
  };

  /// One placement per Table 2 row, in table order (index i holds row i + 1).
  const std::vector<Placement>& Placements();

  /// Project configuration name for a Tonneau2025Table species name ("e-aq"
  /// -> "e_aq", "OH." -> deg+"OH", ...); "" for "H2O" (the solvent). Fatal-free:
  /// an unknown name returns "?" + name.
  std::string SpeciesName(const std::string& paperName);

  /// Project names of every species that is a reactant of a type-1
  /// reaction-table entry, sorted, without duplicates.
  std::vector<std::string> TypeOneSpecies();
}  // namespace Tonneau2025Reactions

#endif  // Tonneau2025Reactions_h
