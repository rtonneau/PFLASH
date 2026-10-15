/// \file DetectorConstruction.cc
/// \brief Implementation of the DetectorConstruction class for a homogeneous
/// water-box geometry

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

  // Set the default box material
  std::cerr << "Setting default material to G4_WATER\n";
  std::cerr.flush();
  this->SetMaterial("G4_WATER");

  std::cerr << "SetMaterial Done\n";
  std::cerr.flush();

  // Create the messenger
  // fDetectorMessenger = new DetectorMessenger(this);

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

G4Material *DetectorConstruction::OtherMaterial(G4String materialName)
{
  G4Material *material(0);

  // Water is defined from NIST material database
  G4NistManager *man = G4NistManager::Instance();
  material = man->FindOrBuildMaterial(materialName);
  // If one wishes to test other density value for water material,
  // one should use instead:
  // G4Material * H2O = man->BuildMaterialWithNewDensity(
  // "G4_WATER_MODIFIED",
  // "G4_WATER",1000*g/cm/cm/cm);
  // Note: any string for "G4_WATER_MODIFIED" parameter is accepted
  // and "G4_WATER" parameter should not be changed
  // Both materials are created and can be selected from dna.mac

  return material;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume *DetectorConstruction::ConstructDetector()
{
  // this->SetMaterial("G4_WATER");
  //  G4Material *water = OtherMaterial("G4_WATER");

  // WORLD VOLUME
  // Geometry follows the chemistry domain so the tracking box and the
  // diffusion boundary stay in sync (default: 1 mm cube).
  const G4DNABoundingBox *boundary = this->fpChemistryWorld->GetChemistryBoundary();
  this->fWorldSizeX = 2. * boundary->halfSideLengthInX();
  this->fWorldSizeY = 2. * boundary->halfSideLengthInY();
  this->fWorldSizeZ = 2. * boundary->halfSideLengthInZ();
  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DetectorConstruction] world = water box of side " +
                     std::to_string(this->fWorldSizeX / um) + " um (/chem/env/halfBox)");

  G4Box *solidWorld = new G4Box("World",               // its name
                                this->fWorldSizeX / 2, // its size
                                this->fWorldSizeY / 2,
                                this->fWorldSizeZ / 2);

  this->fLogicWorld = new G4LogicalVolume(solidWorld,      // its solid
                                          this->fMaterial, // its material
                                          "World");        // its name

  this->fPhysWorld = new G4PVPlacement(0,                 // no rotation
                                       G4ThreeVector(),   // at (0,0,0)
                                       "World",           // its name
                                       this->fLogicWorld, // its logical volume
                                       0,                 // its mother  volume
                                       false,             // no boolean operation
                                       0);                // copy number

  // Visualization attributes
  G4VisAttributes *worldVisAtt = new G4VisAttributes(G4Colour(0.0, 0.4, 0.8, 0.5));
  worldVisAtt->SetVisibility(true);
  this->fLogicWorld->SetVisAttributes(worldVisAtt);

  return this->fPhysWorld;
}

void DetectorConstruction::ConstructSDandField()
{

  G4SDManager::GetSDMpointer()->SetVerboseLevel(1);

  // declare World as a MultiFunctionalDetector scorer
  //
  G4MultiFunctionalDetector *mfDetector = new G4MultiFunctionalDetector("mfDetector");

  //--
  // Kill primary track after a chosen energy loss OR under a chosen
  // kinetic energy

  PrimaryKiller *primaryKiller = new PrimaryKiller("PrimaryKiller");
  primaryKiller->SetMinLossEnergyLimit(500 * keV); // default value
  primaryKiller->SetMaxLossEnergyLimit(1. * MeV);  // default value
  mfDetector->RegisterPrimitive(primaryKiller);

  G4VPrimitiveScorer *primitivSpecies = new ScoreSpecies("Species");
  mfDetector->RegisterPrimitive(primitivSpecies);

  G4SDManager::GetSDMpointer()->AddNewDetector(mfDetector);
  DetectorConstruction::SetSensitiveDetector("World", mfDetector);
}

void DetectorConstruction::SetMaterial(G4String materialChoice)
{
  // Search the material by its name
  G4Material *mat = G4NistManager::Instance()->FindOrBuildMaterial(materialChoice);

  std::cerr << "Within SetMaterial\n";
  std::cerr.flush();
  if (mat)
  {
    std::cerr << "Material found: " << materialChoice << "\n";
    std::cerr.flush();
    if (this->fMaterial != mat)
    {
      std::cerr << "Different from previous material: \n";
      std::cerr.flush();
      this->fMaterial = mat;
      std::cerr << "Material set to: " << this->fMaterial->GetName() << "\n";
      std::cerr.flush();
      if (this->fLogicWorld)
      {
        std::cerr << "Updating logical world material\n";
        std::cerr.flush();
        this->fLogicWorld->SetMaterial(mat);
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
    // Warning the user this material does not exist
    std::stringstream sstr;
    if (this->fMaterial)
    {
      sstr << "material " << +materialChoice << " does not exist, keeping material "
           << this->fMaterial->GetName();

      G4Exception("DetectorConstruction::SetMaterial", "NoWorldMat", JustWarning, sstr.str().c_str());
    }
    else
    {
      sstr << "material " << +materialChoice << " does not exist, and no default material set yet";
      G4Exception("DetectorConstruction::SetMaterial", "NoWorldMat", FatalException, sstr.str().c_str());
    }
  }
}