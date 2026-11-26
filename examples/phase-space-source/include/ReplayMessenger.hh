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
//
// `/run/beamOn N` then replays global histories k..k+N-1. The settings live in
// the process-wide ReplaySource, so the commands are not broadcast to the
// worker threads.
#pragma once

#include <memory>

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;

namespace phsp
{

class ReplaySource;

class ReplayMessenger : public G4UImessenger
{
 public:
  explicit ReplayMessenger(ReplaySource& source);
  ~ReplayMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String value) override;

 private:
  ReplaySource& fSource;
  std::unique_ptr<G4UIdirectory> fPhspDirectory;
  std::unique_ptr<G4UIdirectory> fSourceDirectory;
  std::unique_ptr<G4UIdirectory> fGeomDirectory;
  std::unique_ptr<G4UIcmdWithAString> fAddFileCmd;
  std::unique_ptr<G4UIcmdWithAString> fFirstCmd;
  std::unique_ptr<G4UIcmdWithAString> fBaseSeedCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fBoxSideCmd;
};

}  // namespace phsp
