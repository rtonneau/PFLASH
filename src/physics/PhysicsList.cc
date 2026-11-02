/// \file PhysicsList.cc
/// Configures EM-DNA option2 and the selected Chemistry in dependency order.
/// Dependencies: Geant4 and DnaChemistryList; not standard-library-only.
/// Thread safety: Shared physics list; use only through Geant4 initialization callbacks.

#include "physics/PhysicsList.hh"

#include "G4ChemTimeStepModel.hh"
#include "G4EmDNAPhysics.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmParameters.hh"
#include "G4ProductionCutsTable.hh"
#include "G4SystemOfUnits.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PhysicsList::PhysicsList()
  : G4VModularPhysicsList(),
    fEmDNAPhysicsList(new G4EmDNAPhysics_option2(/*verbose*/ 0)),
    fEmDNAChemistryList(new DnaChemistryList)
{
  const G4double defaultCut_mm = 1. * nanometer;
  G4ProductionCutsTable::GetProductionCutsTable()->SetEnergyRange(100 * eV, 1 * GeV);
  SetDefaultCutValue(defaultCut_mm);
  SetVerboseLevel(0);

  // Default chemistry time-step model: IRT_syn (particle-based stage before
  // the mesoscopic hand-over, see TimeStepAction). A macro may select SBS
  // with /process/chem/TimeStepModel before /run/initialize (no mesoscopic
  // stage); IRT is fatal at /run/initialize (DnaChemistryList).
  G4EmParameters::Instance()->SetTimeStepModel(G4ChemTimeStepModel::IRT_syn);
  G4EmParameters::Instance()->SetVerbose(0);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::ConstructParticle()
{
  fEmDNAPhysicsList->ConstructParticle();
  fEmDNAChemistryList->ConstructParticle();  // -> DnaChemistryList::ConstructMolecule()
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PhysicsList::ConstructProcess()
{
  AddTransportation();
  fEmDNAPhysicsList->ConstructProcess();
  // Chemistry inspects/extends EM processes (including electron solvation),
  // so keep its construction after transport and EM-DNA registration.
  fEmDNAChemistryList->ConstructProcess();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
