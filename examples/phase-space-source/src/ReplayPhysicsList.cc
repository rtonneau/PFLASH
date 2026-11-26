#include "ReplayPhysicsList.hh"

#include "RecordPhysicsList.hh"
#include "ReplayDetectorConstruction.hh"

#include "G4EmDNAPhysicsActivator.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmParameters.hh"
#include "G4EmStandardPhysics_option4.hh"

namespace phsp
{

const char* ReplayPhysicsName(ReplayPhysics physics)
{
  return physics == ReplayPhysics::kStage1 ? "stage1" : "option2";
}

const char* ReplayPhysicsDescription(ReplayPhysics physics)
{
  return physics == ReplayPhysics::kStage1
             ? "stage-1 setup: G4EmStandardPhysics_option4 + DNA_Opt2 in DnaRegion (the box) via "
               "G4EmParameters::AddDNA + G4EmDNAPhysicsActivator, stage-1 cuts"
             : "G4EmDNAPhysics_option2 constructor everywhere, stage-1 cuts";
}

ReplayPhysicsList::ReplayPhysicsList(ReplayPhysics physics) : fPhysics(physics)
{
  SetDefaultCutValue(RecordPhysicsList::kDefaultCut);
  if (physics == ReplayPhysics::kStage1)
  {
    // Same order as RecordPhysicsList: option4's constructor resets
    // G4EmParameters (SetDefaults), which would drop a DNA region added before.
    RegisterPhysics(new G4EmStandardPhysics_option4());
    G4EmParameters::Instance()->AddDNA(ReplayDetectorConstruction::kDnaRegionName, RecordPhysicsList::kDnaOption);
    RegisterPhysics(new G4EmDNAPhysicsActivator());
  }
  else
  {
    RegisterPhysics(new G4EmDNAPhysics_option2());
  }
}

}  // namespace phsp
