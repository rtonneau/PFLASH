/// \file DnaChemistryWorld.cc
/// \brief Implementation of the DnaChemistryWorld class

#include "geometry/DnaChemistryWorld.hh"

#include "core/DnaLogger.hh"
#include "geometry/ScavengerMessenger.hh"

#include "G4ApplicationState.hh"
#include "G4DNABoundingBox.hh"
#include "G4GenericMessenger.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeTable.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryWorld::DnaChemistryWorld()
{
  fMessenger = std::make_unique<G4GenericMessenger>(this, "/chem/env/",
                                                    "Bulk chemistry environment");
  auto& pHCmd = fMessenger->DeclareProperty("pH", fpH, "Bulk water pH (default 7).");
  pHCmd.SetStates(G4State_PreInit);
  auto& halfBoxCmd = fMessenger->DeclareMethodWithUnit(
    "halfBox", "um", &DnaChemistryWorld::SetHalfBox,
    "Half side of the water box, i.e. of the whole geometry (default 500 um).");
  halfBoxCmd.SetStates(G4State_PreInit);

  // After fMessenger: /chem/env/scavenger attaches to the directory it created.
  fScavengerMessenger = std::make_unique<ScavengerMessenger>(this);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryWorld::~DnaChemistryWorld() = default;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryWorld::ConstructChemistryBoundary()
{
  // G4DNABoundingBox initializer-list order is {xhi, xlo, yhi, ylo, zhi, zlo}.
  const std::initializer_list<G4double> box{
    fHalfBox, -fHalfBox, fHalfBox, -fHalfBox, fHalfBox, -fHalfBox};
  fpChemistryBoundary = std::make_unique<G4DNABoundingBox>(box);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryWorld::SetHalfBox(G4double halfBox)
{
  if (halfBox <= 0.) {
    G4Exception("DnaChemistryWorld::SetHalfBox", "InvalidHalfBox", FatalException,
                ("/chem/env/halfBox: the half side must be > 0 (got " +
                 std::to_string(halfBox / um) + " um).")
                  .c_str());
    return;
  }
  fHalfBox = halfBox;
  // The boundary was built in DetectorConstruction's constructor, before any
  // macro: rebuild it so the world, diffusion boundary and scavenger volume
  // all follow the new size.
  ConstructChemistryBoundary();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryWorld::ConstructChemistryComponents()
{
  auto* table = G4MoleculeTable::Instance();

  auto* H2O = table->GetConfiguration("H2O(B)");
  auto* H3OpB = table->GetConfiguration("H3Op(B)");
  auto* OHmB = table->GetConfiguration("OHm(B)");
  if (H2O == nullptr || H3OpB == nullptr || OHmB == nullptr) {
    G4Exception("DnaChemistryWorld::ConstructChemistryComponents", "NoBulkSpecies",
                FatalException,
                "Bulk \"(B)\" configurations are missing - this must run after "
                "DnaChemistryList::ConstructMolecule().");
    return;
  }

  const G4double pKw = 14.0;  // at 25 C
  fpChemicalComponent[H2O] = 55.3 / (mole * liter);
  fpChemicalComponent[H3OpB] = std::pow(10.0, -fpH) / (mole * liter);
  fpChemicalComponent[OHmB] = std::pow(10.0, -(pKw - fpH)) / (mole * liter);

  G4String scavengerSummary;
  for (const auto& scavenger : fScavengers) {
    if (scavenger.molarity <= 0.) {
      continue;  // 0 = absent
    }
    // mustExist = false: report our own error instead of Geant4's generic one.
    auto* conf = table->GetConfiguration(scavenger.species, false);
    if (conf == nullptr) {
      G4Exception("DnaChemistryWorld::ConstructChemistryComponents", "UnknownScavenger",
                  FatalException,
                  ("/chem/env/scavenger: species \"" + scavenger.species +
                   "\" is not in the molecule table.")
                    .c_str());
      return;
    }
    fpChemicalComponent[conf] = scavenger.molarity / (mole * liter);
    scavengerSummary +=
      ", " + scavenger.species + " = " + std::to_string(scavenger.molarity) + " M";
  }

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryWorld] bulk composition: pH = " + std::to_string(fpH) +
                     scavengerSummary + " (" + std::to_string(fpChemicalComponent.size()) +
                     " components)");
}
