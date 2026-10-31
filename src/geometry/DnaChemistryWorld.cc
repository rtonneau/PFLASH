/// \file DnaChemistryWorld.cc
/// Defines the chemistry boundary and bulk water/scavenger composition.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Configure on the master before worker reads; follow Geant4 initialization callbacks.

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
    fHalfBox_mm, -fHalfBox_mm, fHalfBox_mm, -fHalfBox_mm, fHalfBox_mm, -fHalfBox_mm};
  fpChemistryBoundary = std::make_unique<G4DNABoundingBox>(box);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryWorld::SetHalfBox(G4double halfBox_mm)
{
  if (halfBox_mm <= 0.) {
    G4Exception("DnaChemistryWorld::SetHalfBox", "InvalidHalfBox", FatalException,
                ("/chem/env/halfBox: the half side must be > 0 (got " +
                 std::to_string(halfBox_mm / um) + " um).")
                  .c_str());
    return;
  }
  fHalfBox_mm = halfBox_mm;
  // The boundary was built in DetectorConstruction's constructor, before any
  // macro: rebuild it so the world, diffusion boundary and scavenger volume
  // all follow the new size.
  ConstructChemistryBoundary();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryWorld::ConstructChemistryComponents()
{
  auto* moleculeTable = G4MoleculeTable::Instance();

  auto* H2O = moleculeTable->GetConfiguration("H2O(B)");
  auto* H3OpB = moleculeTable->GetConfiguration("H3Op(B)");
  auto* OHmB = moleculeTable->GetConfiguration("OHm(B)");
  if (H2O == nullptr || H3OpB == nullptr || OHmB == nullptr) {
    G4Exception("DnaChemistryWorld::ConstructChemistryComponents", "NoBulkSpecies",
                FatalException,
                "Bulk \"(B)\" configurations are missing - this must run after "
                "DnaChemistryList::ConstructMolecule().");
    return;
  }

  // Keep the water/pH buffer constants from the Geant4 UHDR ChemistryWorld.
  const G4double pKw = 14.0;
  const G4double waterConcentration_M = 55.3;
  fpChemicalComponent[H2O] = waterConcentration_M / (mole * liter);
  fpChemicalComponent[H3OpB] = std::pow(10.0, -fpH) / (mole * liter);
  fpChemicalComponent[OHmB] = std::pow(10.0, -(pKw - fpH)) / (mole * liter);

  G4String scavengerSummary;
  for (const auto& scavenger : fScavengers) {
    if (scavenger.molarity <= 0.) {
      continue;  // 0 = absent
    }
    // mustExist = false: report our own error instead of Geant4's generic one.
    auto* configuration = moleculeTable->GetConfiguration(scavenger.species, false);
    if (configuration == nullptr) {
      G4Exception("DnaChemistryWorld::ConstructChemistryComponents", "UnknownScavenger",
                  FatalException,
                  ("/chem/env/scavenger: species \"" + scavenger.species +
                   "\" is not in the molecule table.")
                    .c_str());
      return;
    }
    fpChemicalComponent[configuration] = scavenger.molarity / (mole * liter);
    scavengerSummary +=
      ", " + scavenger.species + " = " + std::to_string(scavenger.molarity) + " M";
  }

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryWorld] bulk composition: pH = " + std::to_string(fpH) +
                     scavengerSummary + " (" + std::to_string(fpChemicalComponent.size()) +
                     " components)");
}
