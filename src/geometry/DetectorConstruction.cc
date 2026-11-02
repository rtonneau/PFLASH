/// \file DetectorConstruction.cc
/// Builds the homogeneous water world and registers its scorers.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Shared geometry; master builds volumes, and scorer setup
/// follows Geant4's per-thread initialization callbacks.


#include "geometry/DetectorConstruction.hh"

#include "core/DnaLogger.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "scoring/PrimaryKiller.hh"
#include "scoring/ScoreSpecies.hh"

#include "G4DNABoundingBox.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ProductionCuts.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserLimits.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VisAttributes.hh"
#include "G4SDManager.hh"
#include "G4MultiFunctionalDetector.hh"
#include "G4RunManager.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::DetectorConstruction() : G4VUserDetectorConstruction()
{

  std::cerr << "Setting default material to G4_WATER\n";
  std::cerr.flush();
  this->SetMaterial("G4_WATER");

  std::cerr << "SetMaterial Done\n";
  std::cerr.flush();

  // Chemical domain: created here so its diffusion boundary is available
  // before physics/chemistry initialization (DnaChemistryList reads it).
  this->fpChemistryWorld = std::make_unique<DnaChemistryWorld>();
  this->fpChemistryWorld->ConstructChemistryBoundary();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::~DetectorConstruction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume *DetectorConstruction::Construct()
{
  return ConstructDetector();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume *DetectorConstruction::ConstructDetector()
{
  // Geometry follows the chemistry domain so the tracking box and the
  // diffusion boundary stay in sync (default: 1 mm cube).
  const G4DNABoundingBox *boundary = this->fpChemistryWorld->GetChemistryBoundary();
  this->fWorldSizeX_mm = 2. * boundary->halfSideLengthInX();
  this->fWorldSizeY_mm = 2. * boundary->halfSideLengthInY();
  this->fWorldSizeZ_mm = 2. * boundary->halfSideLengthInZ();
  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DetectorConstruction] world = water box of side " +
                     std::to_string(this->fWorldSizeX_mm / um) + " um (/chem/env/halfBox)");

  G4Box *solidWorld = new G4Box("World",
                                this->fWorldSizeX_mm / 2,
                                this->fWorldSizeY_mm / 2,
                                this->fWorldSizeZ_mm / 2);

  this->fLogicWorld = new G4LogicalVolume(solidWorld,
                                          this->fMaterial,
                                          "World");

  this->fPhysWorld = new G4PVPlacement(0,                 // no rotation
                                       G4ThreeVector(),   // at (0,0,0)
                                       "World",
                                       this->fLogicWorld,
                                       0,                 // its mother  volume
                                       false,             // no boolean operation
                                       0);                // copy number

  G4VisAttributes *worldVisAtt = new G4VisAttributes(G4Colour(0.0, 0.4, 0.8, 0.5));
  worldVisAtt->SetVisibility(true);
  this->fLogicWorld->SetVisAttributes(worldVisAtt);

  return this->fPhysWorld;
}

void DetectorConstruction::ConstructSDandField()
{

  G4SDManager::GetSDMpointer()->SetVerboseLevel(1);

  G4MultiFunctionalDetector *mfDetector = new G4MultiFunctionalDetector("mfDetector");

  // Preserve the scorer registration order: Run currently uses the collection
  // ID as the primitive index. Changing the order needs a separate correctness fix.

  PrimaryKiller *primaryKiller = new PrimaryKiller("PrimaryKiller");
  primaryKiller->SetMinLossEnergyLimit(500 * keV); // default value
  primaryKiller->SetMaxLossEnergyLimit(1. * MeV);  // default value
  mfDetector->RegisterPrimitive(primaryKiller);

  G4VPrimitiveScorer *speciesScorer = new ScoreSpecies("Species");
  mfDetector->RegisterPrimitive(speciesScorer);

  G4SDManager::GetSDMpointer()->AddNewDetector(mfDetector);
  DetectorConstruction::SetSensitiveDetector("World", mfDetector);
}

void DetectorConstruction::SetMaterial(G4String materialChoice)
{
  G4Material *material = G4NistManager::Instance()->FindOrBuildMaterial(materialChoice);

  std::cerr << "Within SetMaterial\n";
  std::cerr.flush();
  if (material)
  {
    std::cerr << "Material found: " << materialChoice << "\n";
    std::cerr.flush();
    if (this->fMaterial != material)
    {
      std::cerr << "Different from previous material: \n";
      std::cerr.flush();
      this->fMaterial = material;
      std::cerr << "Material set to: " << this->fMaterial->GetName() << "\n";
      std::cerr.flush();
      if (this->fLogicWorld)
      {
        std::cerr << "Updating logical world material\n";
        std::cerr.flush();
        this->fLogicWorld->SetMaterial(material);
      }
      else
      {
        std::cerr << "Logical world not yet defined, material will be set at construction\n";
        std::cerr.flush();
      }
      G4RunManager::GetRunManager()->PhysicsHasBeenModified();
    }
    else
    {
      std::cerr << "Same as previous material, not changing\n";
      std::cerr.flush();
    }
  }
  else
  {
    ReportMissingMaterial(materialChoice);
  }
}

void DetectorConstruction::ReportMissingMaterial(const G4String &materialChoice) const
{
  std::stringstream message;
  if (this->fMaterial)
  {
    message << "material " << +materialChoice << " does not exist, keeping material "
            << this->fMaterial->GetName();

    G4Exception("DetectorConstruction::SetMaterial", "NoWorldMat", JustWarning, message.str().c_str());
  }
  else
  {
    message << "material " << +materialChoice << " does not exist, and no default material set yet";
    G4Exception("DetectorConstruction::SetMaterial", "NoWorldMat", FatalException, message.str().c_str());
  }
}
