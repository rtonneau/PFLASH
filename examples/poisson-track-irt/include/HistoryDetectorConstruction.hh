// HistoryDetectorConstruction: the world of a `history` run, one G4_WATER box.
//
// The chemistry needs a G4_WATER world: the molecules are located in it when
// they are built, and the Brownian transport of Geant4-DNA works in water. The
// world is only a container here (no physics transport happens in `history`);
// its half side is worldHalfSideUm of the history config, and every injected
// molecule must lie strictly inside it. The optional reflecting chemistry box
// (halfBoxUm) is a separate, smaller boundary, see ExamplePhysicsList.
#pragma once

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

namespace ptirt
{

class HistoryDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  explicit HistoryDetectorConstruction(G4double halfSide);
  ~HistoryDetectorConstruction() override = default;

  G4VPhysicalVolume* Construct() override;

  G4double GetHalfSide() const { return fHalfSide; }

 private:
  G4double fHalfSide;
};

}  // namespace ptirt
