/// \file PhysicsList.hh
/// \brief Definition of the PhysicsList class
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

class PhysicsList : public G4VModularPhysicsList
{
public:
  explicit PhysicsList();
  ~PhysicsList() override = default;

  void ConstructParticle() override;
  void ConstructProcess() override;

  /// True when the chemical stage is present (queried by StackingAction).
  inline G4bool IsChemistryEnabled() const { return fEmDNAChemistryList != nullptr; }

  /// The single, MT-shared chemistry stage instance (queried by RunAction to
  /// apply any /chem/reaction/timeBins* macro configuration).
  inline const DnaChemistryList* GetChemistryList() const { return fEmDNAChemistryList.get(); }

private:
  std::unique_ptr<G4VPhysicsConstructor> fEmDNAPhysicsList;
  std::unique_ptr<DnaChemistryList> fEmDNAChemistryList;
};
#endif
