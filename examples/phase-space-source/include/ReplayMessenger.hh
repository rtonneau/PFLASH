// ReplayMessenger: the commands of phsp-replay (stage 2).
//
//   /phsp/source/addFile <path>      PreInit, repeatable: append a phase-space
//                                    file (.phsp) to the ordered list
//   /phsp/source/first <k>           PreInit or Idle: global index of the first
//                                    history of the next /run/beamOn (default 0)
//   /phsp/source/baseSeed <s>        PreInit: stage-2 base seed, unsigned
//                                    64-bit (default 1; ADR 0015)
//   /phsp/geom/boxSide <value> <unit> PreInit, optional: the expected box side;
//                                    a mismatch with the files is fatal at
//                                    /run/initialize
//   /phsp/geom/chemMargin <v> <unit> PreInit: water margin around the box in
//                                    which the molecules diffuse (default 2 um,
//                                    >= 0; physics tracks never enter it)
//   /phsp/chem/enable <bool>         PreInit: stage-2 chemistry on (default) or
//                                    off (physics only, as ticket 03)
//   /phsp/chem/pH <value>            PreInit: pH of the acid-base buffer,
//                                    in (0, 14), default 7
//   /phsp/chem/o2 <value> <unit>     PreInit: dissolved O2, unit M, mM, uM or
//                                    % (of O2 saturation, kH = 1.3 mM, so
//                                    21 % = 0.273 mM); default 0 (absent); a
//                                    negative value or another unit is fatal
//
// `/run/beamOn N` then replays global histories k..k+N-1. The chemistry end
// time is /scheduler/endTime after /run/initialize (default 1 us). The
// settings live in the process-wide ReplaySource and ReplayChemistrySettings,
// so the commands are not broadcast to the worker threads.
#pragma once

#include <memory>

#include "G4UImessenger.hh"

class G4UIcommand;
class G4UIdirectory;
class G4UIcmdWithABool;
class G4UIcmdWithADouble;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;

namespace phsp
{

class ReplaySource;
struct ReplayChemistrySettings;

class ReplayMessenger : public G4UImessenger
{
 public:
  ReplayMessenger(ReplaySource& source, ReplayChemistrySettings& chemistry);
  ~ReplayMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String value) override;

 private:
  ReplaySource& fSource;
  ReplayChemistrySettings& fChemistry;
  std::unique_ptr<G4UIdirectory> fPhspDirectory;
  std::unique_ptr<G4UIdirectory> fSourceDirectory;
  std::unique_ptr<G4UIdirectory> fGeomDirectory;
  std::unique_ptr<G4UIcmdWithAString> fAddFileCmd;
  std::unique_ptr<G4UIcmdWithAString> fFirstCmd;
  std::unique_ptr<G4UIcmdWithAString> fBaseSeedCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fBoxSideCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fChemMarginCmd;
  std::unique_ptr<G4UIdirectory> fChemDirectory;
  std::unique_ptr<G4UIcmdWithABool> fChemEnableCmd;
  std::unique_ptr<G4UIcmdWithADouble> fPhCmd;
  std::unique_ptr<G4UIcommand> fO2Cmd;
};

}  // namespace phsp
