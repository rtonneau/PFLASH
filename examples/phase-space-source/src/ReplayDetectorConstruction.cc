#include "ReplayDetectorConstruction.hh"

#include "PhaseSpaceSource.hh"
#include "RecordPhysicsList.hh"
#include "ReplayChemistryList.hh"
#include "ReplayMessenger.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4Region.hh"

namespace phsp
{

ReplayDetectorConstruction::ReplayDetectorConstruction(ReplaySource& source, ReplayChemistrySettings& chemistry)
  : fSource(source), fChemistry(chemistry), fMessenger(std::make_unique<ReplayMessenger>(source, chemistry))
{}

ReplayDetectorConstruction::~ReplayDetectorConstruction() = default;

G4VPhysicalVolume* ReplayDetectorConstruction::Construct()
{
  fSource.Open();  // FatalException on a bad file list or a box-side mismatch

  auto* nist = G4NistManager::Instance();
  G4Material* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* water = nist->FindOrBuildMaterial("G4_WATER");

  const G4double halfBox = 0.5 * fSource.BoxSide();
  const G4double margin = fChemistry.chemMargin;
  const G4double halfWorld = halfBox + margin + kWorldMargin;
  auto* worldSolid = new G4Box("World", halfWorld, halfWorld, halfWorld);
  auto* worldLogical = new G4LogicalVolume(worldSolid, vacuum, "World");
  auto* worldPhysical = new G4PVPlacement(nullptr, {}, worldLogical, "World", nullptr, false, 0, true);

  // The water margin (world region), mother of the box.
  G4LogicalVolume* boxMother = worldLogical;
  if (margin > 0)
  {
    const G4double halfMargin = halfBox + margin;
    auto* marginSolid = new G4Box("ChemMargin", halfMargin, halfMargin, halfMargin);
    auto* marginLogical = new G4LogicalVolume(marginSolid, water, "ChemMargin");
    new G4PVPlacement(nullptr, {}, marginLogical, "ChemMargin", worldLogical, false, 0, true);
    boxMother = marginLogical;
  }

  auto* boxSolid = new G4Box("ScoringBox", halfBox, halfBox, halfBox);
  auto* boxLogical = new G4LogicalVolume(boxSolid, water, "ScoringBox");
  new G4PVPlacement(nullptr, {}, boxLogical, "ScoringBox", boxMother, false, 0, true);

  auto* region = new G4Region(kDnaRegionName);
  region->AddRootLogicalVolume(boxLogical);
  RecordPhysicsList::SetDnaRegionCuts(region);

  fBoxLogical = boxLogical;
  fBoxHalfSide = halfBox;
  fChemMargin = margin;
  return worldPhysical;
}

}  // namespace phsp
