// ReplayPhysicsList: the stage-2 physics of phsp-replay (phsp-replay --physics).
//
//   stage1   (default) the stage-1 setup of RecordPhysicsList, applied to the
//            box: G4EmStandardPhysics_option4, then
//            G4EmParameters::AddDNA("DnaRegion", "DNA_Opt2"), then
//            G4EmDNAPhysicsActivator, with the same default cut
//            (RecordPhysicsList::kDefaultCut) and the same region cuts
//            (RecordPhysicsList::SetDnaRegionCuts, applied by
//            ReplayDetectorConstruction). Inside the box this is the stage-1
//            physics by construction, including the EM parameters (atomic
//            de-excitation: fluorescence on, Auger off, cuts not ignored).
//   option2  the G4EmDNAPhysics_option2 constructor everywhere, with the same
//            cuts. Same e- DNA models below 1 MeV, but its constructor also
//            switches on Auger electrons and makes de-excitation ignore the
//            cuts (G4EmDNAPhysics), and it builds the gamma processes of
//            G4EmDNABuilder instead of option4's. Kept for the comparison of
//            README.md ("Stage-2 physics").
//
// Why stage1 is the default: the energy check compares stage-2 deposits with
// the stage-1 in-box deposits of the same histories; the same physics in the
// box makes that comparison exact in the mean. See README.md for the measured
// process and model lists of both stages.
#pragma once

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

namespace phsp
{

enum class ReplayPhysics
{
  kStage1,   // option4 + DNA_Opt2 in DnaRegion (G4EmDNAPhysicsActivator), as phsp-record
  kOption2,  // G4EmDNAPhysics_option2 constructor
};

const char* ReplayPhysicsName(ReplayPhysics physics);
const char* ReplayPhysicsDescription(ReplayPhysics physics);

class ReplayPhysicsList : public G4VModularPhysicsList
{
 public:
  explicit ReplayPhysicsList(ReplayPhysics physics);
  ~ReplayPhysicsList() override = default;
  ReplayPhysics Physics() const { return fPhysics; }

 private:
  ReplayPhysics fPhysics;
};

}  // namespace phsp
