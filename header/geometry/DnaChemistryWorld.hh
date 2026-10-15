/// \file DnaChemistryWorld.hh
/// \brief Definition of the DnaChemistryWorld class
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

class DnaChemistryWorld : public G4VChemistryWorld
{
public:
  DnaChemistryWorld();
  ~DnaChemistryWorld() override;

  void ConstructChemistryBoundary() override;
  void ConstructChemistryComponents() override;

  void SetpH(G4double pH) { fpH = pH; }
  G4double GetpH() const { return fpH; }

  /// Sets the half side of the water box and rebuilds the boundary. PreInit
  /// only: G4DNAScavengerProcess keeps a reference to the boundary from
  /// /run/initialize on. A value <= 0 is a FatalException.
  void SetHalfBox(G4double halfBox);
  G4double GetHalfBox() const { return fHalfBox; }

  /// Adds or replaces (last wins) one exogenous bulk scavenger.
  void SetScavenger(const ScavengerSpec::Entry& entry) { ScavengerSpec::Upsert(fScavengers, entry); }
  const ScavengerSpec::List& GetScavengers() const { return fScavengers; }

private:
  std::unique_ptr<G4GenericMessenger> fMessenger;
  std::unique_ptr<ScavengerMessenger> fScavengerMessenger;
  G4double fpH = 7.0;
  G4double fHalfBox = 500. * um;
  ScavengerSpec::List fScavengers;
};

#endif // DnaChemistryWorld_h
