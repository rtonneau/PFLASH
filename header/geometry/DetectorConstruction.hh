#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1
#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"

#include <memory>

class G4Material;
class G4VChemistryWorld;

class DetectorConstruction : public G4VUserDetectorConstruction
{
public:
  DetectorConstruction();
  virtual ~DetectorConstruction();

  virtual G4VPhysicalVolume *Construct() override;
  void ConstructSDandField() override;

  /** @brief Chemical domain (diffusion boundary + bulk composition).
   *
   * Owned here; created in the constructor so its boundary is available
   * before physics/chemistry initialization. Consumed by DnaChemistryList
   * and (later) by the scavenger material / scorers. */
  G4VChemistryWorld *GetChemistryWorld() const { return fpChemistryWorld.get(); }

  /** @brief Returns a pointer to the world material */
  G4Material *GetMaterial() { return fMaterial; }

  /** @brief Method to set the world material
   *
   * @note there will not be any different material throughout the
   * whole simulation
   *
   * @param mat string containing the name of the material
   * according to NIST database */
  void SetMaterial(G4String mat);

  G4double GetSizeX() const { return this->fWorldSizeX; }
  G4double GetSizeY() const { return this->fWorldSizeY; }
  G4double GetSizeZ() const { return this->fWorldSizeZ; }

private:
  G4Material *fMaterial = nullptr;
  G4Material *OtherMaterial(G4String materialName);
  G4VPhysicalVolume *ConstructDetector();

  std::unique_ptr<G4VChemistryWorld> fpChemistryWorld;

  G4LogicalVolume *fLogicWorld = nullptr;
  G4VPhysicalVolume *fPhysWorld = nullptr;

  G4double fWorldSizeX = 1. * mm;
  G4double fWorldSizeY = 1. * mm;
  G4double fWorldSizeZ = 1. * mm;
};
#endif
