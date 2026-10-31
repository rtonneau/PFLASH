/// \file PhysicsList.hh
/// Configures EM-DNA option2 and the selected Chemistry in dependency order.
/// Dependencies: Geant4 and DnaChemistryList; not standard-library-only.
/// Thread safety: Shared physics list; use only through Geant4 initialization callbacks.
///
/// Simplified along the lines of the Geant4-DNA UHDR example: instead of
/// string-dispatched `RegisterPhysics` calls, the modular list just holds
/// the EM-DNA physics constructor and the project chemistry constructor
/// and drives their `ConstructParticle()` / `ConstructProcess()` directly.
/// To change the EM-DNA physics option, edit the constructor.

#ifndef PhysicsList_h
#define PhysicsList_h 1

#include "chemistry/DnaChemistryList.hh"

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

#include <memory>

class G4VPhysicsConstructor;

/// Owns the EM and Chemistry constructors and invokes their hooks directly.
class PhysicsList : public G4VModularPhysicsList
{
public:
  /// Installs option2, a 1 nm range cut and IRT_syn defaults before macro configuration.
  /// Requires Geant4 setup on the master; initialization/allocation failures propagate.
  explicit PhysicsList();
  /// Releases owned constructors after worker initialization/use has stopped.
  ~PhysicsList() override = default;

  /// Builds physical particles before Chemistry molecules; Geant4 calls this during setup.
  /// Chemistry configuration errors raise G4Exception; no separate error return.
  void ConstructParticle() override;
  /// Requires constructed particles; registers transport, EM and Chemistry in that order.
  /// Called for each Geant4 process-manager setup; Geant4 exceptions propagate.
  void ConstructProcess() override;

  /// True when the owned Chemistry constructor exists; no runtime-model validation or failure.
  /// Queried by StackingAction before starting event chemistry.
  inline G4bool IsChemistryEnabled() const { return fEmDNAChemistryList != nullptr; }

  /// The single, MT-shared chemistry stage instance (queried by RunAction to
  /// apply any /chem/reaction/timeBins* macro configuration). Borrowed for this list's
  /// lifetime; no failure path. Configure only in the Geant4 initialization/run lifecycle.
  inline const DnaChemistryList* GetChemistryList() const { return fEmDNAChemistryList.get(); }

private:
  std::unique_ptr<G4VPhysicsConstructor> fEmDNAPhysicsList;
  std::unique_ptr<DnaChemistryList> fEmDNAChemistryList;
};
#endif
