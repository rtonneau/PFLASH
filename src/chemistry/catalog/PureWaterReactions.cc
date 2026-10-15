/// \file PureWaterReactions.cc
/// \brief Implementation of the PureWaterReactions reaction-table builder

#include "chemistry/catalog/PureWaterReactions.hh"

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
    G4Exception("PureWaterReactions::BuildPureWaterReactions", "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PureWaterReactions::BuildPureWaterReactions(G4DNAMolecularReactionTable* reactionTable)
{
  // add(): fully diffusion-controlled (reaction type 0, the default).
  // partial(): partially diffusion-controlled, SetReactionType(1) unless
  // ChemistryTypes::PartialReactionsEnabled() is false (SBS: stays type 0) -- the IRT
  // stepper then samples an activation step with the vdW reaction radius
  // (Type II neutral pair, Type IV ionic pair: G4DNAMolecularReactionData.cc).
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
  auto* Om = Conf("Om");
  auto* HO2 = Conf(kHO2);
  auto* HO2m = Conf("HO2m");
  auto* O2m = Conf("O2m");
  auto* O3m = Conf("O3m");
  auto* Oxy = Conf("Oxy");  // O(3P) atom from water dissociation

  // Pure-water radiolysis (project values -- not UHDR's SecondOrderReactionExtended
  // "Type I" block, which covers the same 9 pairs with different rate constants
  // for 7 of them; keeping these avoids overwriting/duplicating SetReaction
  // calls for the same reactant pair).
  // Type 0: G4EmDNAChemistry_option3 Type I (H+H, e_aq+H) / Type III (e_aq+e_aq, H3O+ + OH-).
  add(H, H, 1.2e10, {H2});
  add(e_aq, H, 2.65e10, {H2, OHm});
  add(e_aq, e_aq, 0.5e10, {H2, OHm, OHm});
  add(H3Op, OHm, 1.43e11, {});
  // Type 1: option3 Type II (e_aq+OH, OH+OH, e_aq+H2O2, OH+H) / Type IV (e_aq+H3O+); UHDR same.
  partial(e_aq, OH, 2.95e10, {OHm});
  partial(OH, OH, 0.44e10, {H2O2});
  partial(e_aq, H2O2, 1.41e10, {OHm, OH});
  partial(e_aq, H3Op, 2.11e10, {H});
  partial(OH, H, 1.44e10, {});

  // O2 scavenging against *tracked* radiolytic O2 molecules (UHDR:
  // ChemOxygenWaterBuilder::OxygenScavengerReaction). The same reactions
  // against the dissolved-O2 background are bulk reactions, see the
  // bulk-reaction list below; they share these entries (same pair).
  // Type 1: option3 Type II (eaq-+O2, H+O2, O2+O-); UHDR leaves them 0 as bulk-only reactions.
  partial(e_aq, O2, 1.74e10, {O2m});
  partial(H, O2, 2.1e10, {HO2});
  partial(Om, O2, 3.7e9, {O3m});

  // O2-/HO2/HO2-/O-/O3- second-order network (UHDR:
  // ChemOxygenWaterBuilder::SecondOrderReactionExtended, "extended" block
  // only; NO2-/CO2/HCO3-/N2O/MeOH lines excluded -- out of project scope).
  // Type 0: option3 Type I (H+O-, O(3p)+OH) / Type III (H3O+ + O3-); UHDR same.
  add(H, Om, 2.00e10, {OHm});
  add(H3Op, O3m, 9.0e10, {OH, O2});
  add(Oxy, OH, 2.0e10, {HO2});  // B. Gervais et al., Chem. Phys. Lett. 410 (2005) 330
  // Type 0: O2- + O2- has no option3 counterpart; UHDR leaves it 0.
  add(O2m, O2m, 1.0e2, {H2O2, O2, OHm, OHm});
  // Type 1: option3 Type II / Type IV (all charged pairs below); UHDR same.
  partial(H, HO2, 1.00e10, {H2O2});
  partial(H, O2m, 1.00e10, {HO2m});
  partial(OH, O2m, 1.07e10, {O2, OHm});
  partial(e_aq, O2m, 1.3e10, {H2O2, OHm, OHm});
  partial(e_aq, HO2m, 3.51e9, {Om, OHm});
  partial(e_aq, Om, 2.31e10, {OHm, OHm});
  partial(H3Op, O2m, 4.78e10, {HO2});
  partial(H3Op, HO2m, 4.78e10, {H2O2});
  partial(H3Op, Om, 4.78e10, {OH});
  partial(e_aq, HO2, 1.29e10, {HO2m});
  partial(OH, OHm, 1.27e10, {Om});
  partial(OH, HO2, 7.90e9, {O2});
  partial(OH, HO2m, 8.32e9, {HO2, OHm});
  partial(OH, Om, 1.00e9, {HO2m});
  partial(OH, O3m, 8.50e9, {O2m, HO2});
  partial(OHm, HO2, 1.27e10, {O2m});
  partial(H2O2, OHm, 1.3e10, {HO2m});
  partial(H2O2, Om, 5.55e8, {HO2, OHm});
  partial(H2, Om, 1.21e8, {H, OHm});
  partial(O2m, Om, 6.00e8, {O2, OHm, OHm});
  partial(HO2m, Om, 3.50e8, {O2m, OHm});
  partial(Om, Om, 1.00e8, {H2O2, OHm, OHm});
  partial(Om, O3m, 7.00e8, {O2m, O2m});
  partial(H, OHm, 2.51e7, {e_aq});
  partial(H, H2O2, 3.50e7, {OH});
  partial(OH, H2O2, 2.88e7, {HO2});
  partial(OH, H2, 3.28e7, {H});
  partial(HO2, HO2, 9.80e5, {H2O2, O2});
  partial(HO2, O2m, 9.70e7, {HO2m, O2});
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ChemistryTypes::BulkReactionList PureWaterReactions::BuildPureWaterBulkReactions()
{
  const G4double M = 1e-3 * m3 / (mole * s);  // bimolecular unit (M^-1 s^-1)
  const G4double cW = 55.3;                   // bulk water molarity factor

  // Acid-base buffer equilibria against H3Op(B)/OHm(B)/H2O, plus the
  // dissolved-O2 scavenger reactions: the "O2" partner is the bulk O2 set by
  // /chem/env/scavenger (UHDR: EmDNAChemistry scavenger processes); inert
  // while its concentration is 0.
  return {
    {"H",
     {{"H2O", 6.32 / s, {"e_aq", "H3Op(B)"}, 0},
      {"OHm(B)", 2.49e7 * M, {"e_aq"}, 0},
      {"O2", 2.1e10 * M, {kHO2}, 0}}},
    {"e_aq",
     {{"H3Op(B)", 2.11e10 * M, {"H"}, 0},
      {"H2O", 1.57e1 * cW / s, {"H", "OHm(B)"}, 0},
      {"O2", 1.74e10 * M, {"O2m"}, 0}}},
    {"O2m", {{"H3Op(B)", 4.78e10 * M, {kHO2}, 6}, {"H2O", 0.15 * cW / s, {kHO2, "OHm(B)"}, 0}}},
    {kHO2, {{"OHm(B)", 1.27e10 * M, {"O2m"}, 0}, {"H2O", 7.58e5 / s, {"H3Op(B)", "O2m"}, 6}}},
    {"HO2m",
     {{"H3Op(B)", 4.78e10 * M, {"H2O2"}, 0}, {"H2O", 1.36e6 * cW / s, {"H2O2", "OHm(B)"}, 7}}},
    {"Om",
     {{"H3Op(B)", 9.56e10 * M, {kOH}, 0},
      {"H2O", 1.8e6 * cW / s, {kOH, "OHm(B)"}, 8},
      {"O2", 3.7e9 * M, {"O3m"}, 0}}},
    {"O3m", {{"H3Op(B)", 9.0e10 * M, {kOH, "O2"}, 0}, {"H2O", 2.66e3 / s, {"Om", "O2"}, 0}}},
    {"H2O2", {{"H2O", 7.86e-2 / s, {"HO2m", "H3Op(B)"}, 0}, {"OHm(B)", 1.27e10 * M, {"HO2m"}, 7}}},
    {kOH, {{"OHm(B)", 1.27e10 * M, {"Om"}, 8}, {"H2O", 0.060176635 / s, {"Om", "H3Op(B)"}, 0}}},
    {"OHm", {{"H3Op(B)", 1.13e11 * M, {}, 0}}},
    {"H3Op", {{"OHm(B)", 1.13e11 * M, {}, 0}}},
  };
}
