/// \file Farokhi2023Reactions.cc
/// \brief Implementation of the Farokhi2023Reactions reaction-table builder
///
/// Reproduces the base-water block of Table 2 of Farokhi et al. 2023, ground
/// truth examples/extended/medical/dna/scavenger/Rtable_O2.txt in the Geant4
/// 11.4.1 source tree (not the paper PDF, which has OCR artifacts). No
/// acid-base buffer, by design (the network has none).

#include "chemistry/catalog/Farokhi2023Reactions.hh"

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
    G4Exception("Farokhi2023Reactions::BuildFarokhiReactions", "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void Farokhi2023Reactions::BuildFarokhiReactions(G4DNAMolecularReactionTable* reactionTable)
{
  // add(): fully diffusion-controlled (reaction type 0, the default).
  // partial(): partially diffusion-controlled, SetReactionType(1) unless
  // ChemistryTypes::PartialReactionsEnabled() is false (SBS: stays type 0) -- the IRT
  // stepper then samples an activation step with the vdW reaction radius
  // (Type II neutral pair, Type IV ionic pair: G4DNAMolecularReactionData.cc).
  // Rtable_O2.txt gives explicit type_N labels (Frongillo et al. 1998):
  // type_1/type_3 map to add() (fully diffusion-controlled), type_2/type_4 to
  // partial() (partially diffusion-controlled), the same mapping
  // PureWaterReactions.cc/BoscoloChemReactions.cc use for Pimblott/LaVerne
  // types.
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

  // Rtable_O2.txt, "Reactions between radio-induced species" section, in
  // file order. H2O reactants/products dropped (water is not tracked), as in
  // the existing catalog files.
  // type_3: fully diffusion-controlled.
  add(e_aq, e_aq, 6.36e9, {H2, OHm, OHm});
  // type_2: partially diffusion-controlled.
  partial(e_aq, OH, 2.95e10, {OHm});
  // type_1: fully diffusion-controlled.
  add(e_aq, H, 2.50e10, {H2, OHm});
  // type_4: partially diffusion-controlled.
  partial(e_aq, H3Op, 2.11e10, {H});
  // type_2: partially diffusion-controlled.
  partial(e_aq, H2O2, 1.10e10, {OHm, OH});
  partial(OH, OH, 5.50e9, {H2O2});
  partial(OH, H, 1.55e10, {});
  // type_1: fully diffusion-controlled.
  add(H, H, 5.03e9, {H2});
  // type_3: fully diffusion-controlled.
  add(H3Op, OHm, 1.13e11, {});
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ChemistryTypes::BulkReactionList Farokhi2023Reactions::BuildFarokhiBulkReactions()
{
  const G4double M = 1e-3 * m3 / (mole * s);  // bimolecular unit (M^-1 s^-1)

  // Rtable_O2.txt, "Reactions with the scavengers" section: the dissolved-O2
  // background set by /chem/env/scavenger O2 (inert while its concentration
  // is 0). No acid-base buffer.
  return {
    {"e_aq", {{"O2", 1.74e10 * M, {"O2m"}, 0}}},
    {"H", {{"O2", 2.10e10 * M, {kHO2}, 0}}},
    {"Om", {{"O2", 3.70e9 * M, {"O3m"}, 0}}},
  };
}
