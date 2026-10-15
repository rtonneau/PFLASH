/// \file BoscoloChemReactions.cc
/// \brief Implementation of the BoscoloChemReactions reaction-table builder
///
/// Reproduces Table 1 of Boscolo et al., "Impact of Target Oxygenation on the
/// Chemical Track Evolution of Ion and Electron Radiation", Int. J. Mol. Sci.
/// 2020, 21, 424. No acid-base buffer, by design (the paper has none, see
/// docs/adr/0002-named-chemistries.md).

#include "chemistry/catalog/BoscoloChemReactions.hh"

#include "chemistry/ChemistryTypes.hh"

#include "G4DNAMolecularReactionTable.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeTable.hh"
#include "G4SystemOfUnits.hh"

#include <initializer_list>

namespace
{
// Configuration tags containing the UTF-8 degree sign (0xC2 0xB0), spelled
// from explicit bytes so they match the names stored by
// G4ChemDissociationChannels_option1 whatever this file's source encoding is.
// Duplicated locally (rather than shared with DnaChemistryList.cc) so this
// file has zero project-specific dependencies.
const G4String kOH = G4String("\xC2\xB0") + "OH";   // hydroxyl radical
const G4String kHO2 = G4String("HO2") + "\xC2\xB0"; // hydroperoxyl radical

using MolConf = const G4MolecularConfiguration*;

MolConf Conf(const G4String& name)
{
  auto* p = G4MoleculeTable::Instance()->GetConfiguration(name);
  if (p == nullptr) {
    G4Exception("BoscoloChemReactions::BuildBoscoloChemReactions", "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void BoscoloChemReactions::BuildBoscoloChemReactions(G4DNAMolecularReactionTable* reactionTable)
{
  // add(): fully diffusion-controlled (reaction type 0, the default).
  // partial(): partially diffusion-controlled, SetReactionType(1) unless
  // ChemistryTypes::PartialReactionsEnabled() is false (SBS: stays type 0) -- the IRT
  // stepper then samples an activation step with the vdW reaction radius
  // (Type II neutral pair, Type IV ionic pair: G4DNAMolecularReactionData.cc).
  // The paper gives no reaction types; they follow G4EmDNAChemistry_option3
  // (and UHDR ChemOxygenWaterBuilder, which agrees for every pair below
  // except (xiv)/(xv)).
  auto add = [reactionTable](MolConf a, MolConf b, G4double k,
                             std::initializer_list<MolConf> products) {
    auto* rd = new G4DNAMolecularReactionData(k * (1e-3 * m3 / (mole * s)), a, b);
    for (auto* p : products) {
      rd->AddProduct(p);
    }
    reactionTable->SetReaction(rd);
    return rd;
  };
  auto partial = [&add](MolConf a, MolConf b, G4double k,
                        std::initializer_list<MolConf> products) {
    auto* rd = add(a, b, k, products);
    if (ChemistryTypes::PartialReactionsEnabled()) {
      rd->SetReactionType(1);
    }
  };

  auto* e_aq = Conf("e_aq");
  auto* H = Conf("H");
  auto* H2 = Conf("H2");
  auto* OH = Conf(kOH);
  auto* OHm = Conf("OHm");
  auto* H3Op = Conf("H3Op");
  auto* H2O2 = Conf("H2O2");
  auto* O2 = Conf("O2");
  auto* HO2 = Conf(kHO2);
  auto* HO2m = Conf("HO2m");
  auto* O2m = Conf("O2m");

  // Boscolo et al. 2020, Table 1, reactions (i)-(xxvi), in table order.
  // Rates are the table's k (10^10 dm3 mol-1 s-1) times 1e10; H2O reactants
  // and products are dropped (water is not tracked).
  // Type 1: option3 Type II (i)-(v) / Type IV (viii is ionic, e_aq + H3O+).
  partial(OH, OH, 0.6e10, {H2O2});               // (i)
  partial(OH, e_aq, 2.2e10, {OHm});              // (ii)
  partial(OH, H, 2.0e10, {});                    // (iii)
  partial(OH, H2, 0.0045e10, {H});               // (iv)
  partial(OH, H2O2, 0.0023e10, {HO2});           // (v)
  // Type 0: option3 Type III (vi) / Type I (vii).
  add(e_aq, e_aq, 0.55e10, {H2, OHm, OHm});      // (vi)
  add(e_aq, H, 2.5e10, {H2, OHm});               // (vii)
  // Type 1: option3 Type IV (viii) / Type II (ix).
  partial(e_aq, H3Op, 1.7e10, {H});              // (viii)
  partial(e_aq, H2O2, 1.0e10, {OH, OHm});        // (ix)
  // Type 0: option3 Type I.
  add(H, H, 1.0e10, {H2});                       // (x)
  // Type 1: option3 Type II.
  partial(H, H2O2, 0.01e10, {OH});               // (xi)
  partial(H, OHm, 0.002e10, {e_aq});             // (xii)
  // Type 0: option3 Type III.
  add(H3Op, OHm, 10.0e10, {});                   // (xiii)
  // (xiv)/(xv) against *tracked* radiolytic O2; the same reactions against
  // the dissolved-O2 background are bulk reactions, see the list below; they
  // share these entries (same pair).
  // Type 1: option3 Type II; UHDR leaves them 0 as bulk-only reactions.
  partial(e_aq, O2, 1.9e10, {O2m});              // (xiv)
  partial(H, O2, 2.0e10, {HO2});                 // (xv)
  // Type 1: option3 Type II (xvi)-(xix), (xxi), (xxii), (xxv), (xxvi) /
  // Type IV (xx), (xxiii), (xxiv); UHDR same.
  partial(OH, HO2, 1.0e10, {O2});                // (xvi)
  partial(OH, O2m, 0.9e10, {O2, OHm});           // (xvii)
  partial(OH, HO2m, 0.5e10, {HO2, OHm});         // (xviii)
  partial(e_aq, HO2, 2.0e10, {HO2m});            // (xix)
  partial(e_aq, O2m, 1.3e10, {OHm, HO2m});       // (xx)
  partial(H, HO2, 2.0e10, {H2O2});               // (xxi)
  partial(H, O2m, 2.0e10, {HO2m});               // (xxii)
  partial(H3Op, O2m, 3e10, {HO2});               // (xxiii)
  partial(H3Op, HO2m, 2.0e10, {H2O2});           // (xxiv)
  partial(HO2, HO2, 0.000076e10, {H2O2, O2});    // (xxv)
  partial(HO2, O2m, 0.0085e10, {O2, HO2m});      // (xxvi)
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ChemistryTypes::BulkReactionList BoscoloChemReactions::BuildBoscoloChemBulkReactions()
{
  const G4double M = 1e-3 * m3 / (mole * s);  // bimolecular unit (M^-1 s^-1)

  // Table 1 has no acid-base buffer: only reactions (xiv)/(xv) against the
  // dissolved-O2 background set by /chem/env/scavenger O2 (inert while its
  // concentration is 0).
  return {
    {"e_aq", {{"O2", 1.9e10 * M, {"O2m"}, 0}}},  // (xiv)
    {"H", {{"O2", 2.0e10 * M, {kHO2}, 0}}},      // (xv)
  };
}
