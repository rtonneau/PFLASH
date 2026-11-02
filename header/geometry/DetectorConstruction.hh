/// \file DetectorConstruction.hh
/// Builds the homogeneous water world and registers its scorers.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Shared geometry; master builds volumes, and scorer setup
/// follows Geant4's per-thread initialization callbacks.

#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1
#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"

#include <memory>

class G4Material;
class G4VChemistryWorld;

/// Builds the homogeneous water world and registers its scorers.
class DetectorConstruction : public G4VUserDetectorConstruction
{
public:
  /// Creates G4_WATER and the Chemistry world/boundary; requires a live run manager.
  /// NIST/material initialization failures follow G4Exception handling.
  DetectorConstruction();
  /// Releases the owned Chemistry world after all process/scorer users have stopped.
  virtual ~DetectorConstruction();

  /// Builds the world from the configured Chemistry boundary; call during master geometry setup.
  /// Returns a volume registered with Geant4 stores; Geant4/allocation failures propagate.
  virtual G4VPhysicalVolume *Construct() override;
  /// Requires a constructed World volume; registers this thread's killer and species scorers.
  /// Geant4 owns the registered detectors; registration failures follow G4Exception handling.
  void ConstructSDandField() override;

  /** @brief Chemical domain (diffusion boundary + bulk composition).
   *
   * Owned here; created in the constructor so its boundary is available
   * before physics/chemistry initialization. Consumed by DnaChemistryList
   * and (later) by the scavenger material / scorers. Borrowed for this
   * detector's lifetime; no failure path. */
  G4VChemistryWorld *GetChemistryWorld() const { return fpChemistryWorld.get(); }

  /** @brief Borrowed Geant4-owned world material; valid after successful construction. */
  G4Material *GetMaterial() { return fMaterial; }

  /** @brief Sets the homogeneous world material and marks physics modified.
   * @param mat NIST material name; configure before concurrent tracking.
   * Requires a live run manager. Unknown material warns and preserves an existing
   * material, or raises FatalException if none exists. Geant4 owns the material. */
  void SetMaterial(G4String mat);

  /// Full constructed X side in Geant4 length units (mm); initialized to 1 mm before Construct.
  G4double GetSizeX() const { return this->fWorldSizeX_mm; }
  /// Full constructed Y side in mm; same initialization behavior, no failure path.
  G4double GetSizeY() const { return this->fWorldSizeY_mm; }
  /// Full constructed Z side in mm; same initialization behavior, no failure path.
  G4double GetSizeZ() const { return this->fWorldSizeZ_mm; }

private:
  G4Material *fMaterial = nullptr;
  /// Preserves the existing warning/fatal distinction for a failed material lookup.
  void ReportMissingMaterial(const G4String &materialChoice) const;
  G4VPhysicalVolume *ConstructDetector();

  std::unique_ptr<G4VChemistryWorld> fpChemistryWorld;

  G4LogicalVolume *fLogicWorld = nullptr;
  G4VPhysicalVolume *fPhysWorld = nullptr;

  G4double fWorldSizeX_mm = 1. * mm;
  G4double fWorldSizeY_mm = 1. * mm;
  G4double fWorldSizeZ_mm = 1. * mm;
};
#endif
