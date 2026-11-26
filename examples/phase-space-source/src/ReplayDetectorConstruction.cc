#include "ReplayDetectorConstruction.hh"

#include "PhaseSpaceSource.hh"
#include "RecordPhysicsList.hh"
#include "ReplayMessenger.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4Region.hh"

namespace phsp
{

ReplayDetectorConstruction::ReplayDetectorConstruction(ReplaySource& source)
  : fSource(source), fMessenger(std::make_unique<ReplayMessenger>(source))
{}

ReplayDetectorConstruction::~ReplayDetectorConstruction() = default;

G4VPhysicalVolume* ReplayDetectorConstruction::Construct()
{
  fSource.Open();  // FatalException on a bad file list or a box-side mismatch

  auto* nist = G4NistManager::Instance();
  G4Material* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* water = nist->FindOrBuildMaterial("G4_WATER");

  const G4double halfBox = 0.5 * fSource.BoxSide();
  const G4double halfWorld = halfBox + kWorldMargin;
  auto* worldSolid = new G4Box("World", halfWorld, halfWorld, halfWorld);
  auto* worldLogical = new G4LogicalVolume(worldSolid, vacuum, "World");
  auto* worldPhysical = new G4PVPlacement(nullptr, {}, worldLogical, "World", nullptr, false, 0, true);

  auto* boxSolid = new G4Box("ScoringBox", halfBox, halfBox, halfBox);
  auto* boxLogical = new G4LogicalVolume(boxSolid, water, "ScoringBox");
  new G4PVPlacement(nullptr, {}, boxLogical, "ScoringBox", worldLogical, false, 0, true);

  auto* region = new G4Region(kDnaRegionName);
  region->AddRootLogicalVolume(boxLogical);
  RecordPhysicsList::SetDnaRegionCuts(region);

  fBoxLogical = boxLogical;
  return worldPhysical;
}

}  // namespace phsp
