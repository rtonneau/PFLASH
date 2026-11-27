// RecordMessenger: the PreInit geometry commands of phsp-record.
//
//   /phsp/geom/phantomSide <value> <unit>   side of the water phantom cube (> 0)
//   /phsp/geom/depth <value> <unit>         depth of the box centre below the -z face (> 0)
//   /phsp/geom/boxSide <value> <unit>       side of a scoring box (> 0)
//   /phsp/geom/shell <value> <unit>         DNA water around the box, each side (>= 0)
//   /phsp/geom/latticePitch <value> <unit>  0 = single box, else > boxSide + 2 x shell
//
// Each command checks its own sign; the combination is checked before
// /run/initialize (RecordGeometryProblem). The geometry is built on the master
// only, so the commands are not broadcast to the worker threads.
#pragma once

#include <memory>

#include "G4UImessenger.hh"

class G4UIdirectory;
class G4UIcmdWithADoubleAndUnit;

namespace phsp
{

class RecordDetectorConstruction;

class RecordMessenger : public G4UImessenger
{
 public:
  explicit RecordMessenger(RecordDetectorConstruction* detector);
  ~RecordMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String value) override;

 private:
  RecordDetectorConstruction* fDetector;
  std::unique_ptr<G4UIdirectory> fPhspDirectory;
  std::unique_ptr<G4UIdirectory> fGeomDirectory;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fPhantomSideCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fDepthCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fBoxSideCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fShellCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fLatticePitchCmd;
};

}  // namespace phsp
