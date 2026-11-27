#include "RecordPhysicsList.hh"

#include "RecordDetectorConstruction.hh"

#include "G4EmDNAPhysicsActivator.hh"
#include "G4EmParameters.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4ProductionCuts.hh"
#include "G4Region.hh"

namespace phsp
{

RecordPhysicsList::RecordPhysicsList()
{
  SetDefaultCutValue(kDefaultCut);
  // option4 first: its constructor resets G4EmParameters (SetDefaults), which
  // would drop a DNA region added before it.
  RegisterPhysics(new G4EmStandardPhysics_option4());
  G4EmParameters::Instance()->AddDNA(RecordDetectorConstruction::kDnaRegionName, kDnaOption);
  // Physics type 0 (not bElectromagnetic), so RegisterPhysics keeps it beside
  // option4; its ConstructProcess runs after option4's and replaces the e-
  // models below 1 MeV in the region.
  RegisterPhysics(new G4EmDNAPhysicsActivator());
}

void RecordPhysicsList::SetDnaRegionCuts(G4Region* region)
{
  auto* cuts = new G4ProductionCuts();
  cuts->SetProductionCut(kDnaRegionCut, "gamma");
  cuts->SetProductionCut(kDnaRegionCut, "e-");
  cuts->SetProductionCut(kDnaRegionCut, "e+");
  cuts->SetProductionCut(kDnaRegionProtonCut, "proton");
  region->SetProductionCuts(cuts);
}

}  // namespace phsp
