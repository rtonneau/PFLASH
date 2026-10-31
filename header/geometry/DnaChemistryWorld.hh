/// \file DnaChemistryWorld.hh
/// Defines the chemistry boundary and bulk water/scavenger composition.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Configure on the master before worker reads; follow Geant4 initialization callbacks.
///
/// Project chemical domain: a homogeneous water box (diffusion boundary)
/// plus the bulk / background composition of the solvent (water molarity,
/// H3O+/OH- from pH, and optional exogenous scavengers such as dissolved O2).
/// Modelled on the Geant4-DNA `UHDR` example `ChemistryWorld`.
///
/// UI (all G4State_PreInit, directory /chem/env/):
///   /chem/env/pH <double>                        bulk water pH (default 7)
///   /chem/env/halfBox <value> <unit>             half side of the water box
///       (default 500 um, must be > 0). The box is the whole simulation
///       geometry (world volume), the chemistry diffusion boundary and the
///       bulk-scavenger volume.
///   /chem/env/scavenger <species> <value> <unit> exogenous bulk scavenger;
///       unit M, mM, uM, or % (O2 only, kH = 0.0013 M); repeat = last wins;
///       0 = absent (ScavengerMessenger, ScavengerSpec)
/// A scavenger's reactions are bulk reactions of the selected Chemistry
/// (docs/adr/0004-scavenger-reactions-per-chemistry.md).

#ifndef DnaChemistryWorld_h
#define DnaChemistryWorld_h 1

#include "geometry/ScavengerSpec.hh"

#include "G4SystemOfUnits.hh"
#include "G4VChemistryWorld.hh"
#include "globals.hh"

#include <memory>

class G4GenericMessenger;
class ScavengerMessenger;

/// Defines the chemistry boundary and bulk water/scavenger composition.
class DnaChemistryWorld : public G4VChemistryWorld
{
public:
  /// Registers PreInit UI controls on the master; boundary construction is a separate hook.
  DnaChemistryWorld();
  /// Releases UI controls and the base-owned boundary after all borrowed users stop.
  ~DnaChemistryWorld() override;

  /// Replaces the boundary using the configured half side (mm); invalidates borrowed boundary pointers.
  /// Call before process creation; allocation exceptions propagate.
  void ConstructChemistryBoundary() override;
  /// Requires the shared molecule set; populates bulk concentrations in Geant4 units.
  /// Missing species raise FatalException. Existing component entries are not cleared.
  void ConstructChemistryComponents() override;

  /// Sets dimensionless pH before composition construction; no range/finite validation.
  void SetpH(G4double pH) { fpH = pH; }
  /// Returns configured dimensionless pH; no failure path.
  G4double GetpH() const { return fpH; }

  /// Sets the half side of the water box and rebuilds the boundary. PreInit
  /// only: G4DNAScavengerProcess keeps a reference to the boundary from
  /// /run/initialize on. Input is in mm; a value <= 0 raises FatalException.
  /// Supply a finite value (not checked); direct C++ calls do not enforce PreInit.
  void SetHalfBox(G4double halfBox_mm);
  /// Returns the configured half side in mm; no failure path.
  G4double GetHalfBox() const { return fHalfBox_mm; }

  /// Adds/replaces a scavenger in mol/L before composition construction.
  /// Entry is copied without validation; use ScavengerSpec::Parse first. Allocation may throw.
  void SetScavenger(const ScavengerSpec::Entry& entry) { ScavengerSpec::Upsert(fScavengers, entry); }
  /// Borrowed list for this world lifetime; freeze configuration before worker reads.
  const ScavengerSpec::List& GetScavengers() const { return fScavengers; }

private:
  std::unique_ptr<G4GenericMessenger> fMessenger;
  std::unique_ptr<ScavengerMessenger> fScavengerMessenger;
  G4double fpH = 7.0;
  G4double fHalfBox_mm = 500. * um;
  ScavengerSpec::List fScavengers;
};

#endif // DnaChemistryWorld_h
