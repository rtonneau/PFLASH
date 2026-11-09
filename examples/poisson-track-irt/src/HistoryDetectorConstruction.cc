#include "HistoryDetectorConstruction.hh"

#include "G4Box.hh"
#include "G4Exception.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"

namespace ptirt
{

HistoryDetectorConstruction::HistoryDetectorConstruction(G4double halfSide) : fHalfSide(halfSide)
{
  if (!(halfSide > 0))
    G4Exception("HistoryDetectorConstruction::HistoryDetectorConstruction", "PTIRT_HISTORY_HALFSIDE",
                FatalErrorInArgument, "the world half side must be positive");
}

G4VPhysicalVolume* HistoryDetectorConstruction::Construct()
{
  auto* water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
  auto* solid = new G4Box("HistoryBox", fHalfSide, fHalfSide, fHalfSide);
  auto* logical = new G4LogicalVolume(solid, water, "HistoryBox");
  return new G4PVPlacement(nullptr, {}, logical, "HistoryBox", nullptr, false, 0, true);
}

}  // namespace ptirt
