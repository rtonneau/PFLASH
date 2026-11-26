#include "RecordMessenger.hh"

#include "RecordDetectorConstruction.hh"

#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIdirectory.hh"

namespace phsp
{

namespace
{

std::unique_ptr<G4UIcmdWithADoubleAndUnit> MakeLengthCommand(const char* path, G4UImessenger* messenger,
                                                             const char* guidance, const char* range)
{
  auto command = std::make_unique<G4UIcmdWithADoubleAndUnit>(path, messenger);
  command->SetGuidance(guidance);
  command->SetParameterName("value", false);
  command->SetRange(range);
  command->SetUnitCategory("Length");
  command->AvailableForStates(G4State_PreInit);
  command->SetToBeBroadcasted(false);
  return command;
}

}  // namespace

RecordMessenger::RecordMessenger(RecordDetectorConstruction* detector) : fDetector(detector)
{
  fPhspDirectory = std::make_unique<G4UIdirectory>("/phsp/");
  fPhspDirectory->SetGuidance("Phase-space source example (phsp-record).");
  fGeomDirectory = std::make_unique<G4UIdirectory>("/phsp/geom/");
  fGeomDirectory->SetGuidance("Stage-1 geometry (PreInit; checked together before /run/initialize).");

  fPhantomSideCmd = MakeLengthCommand("/phsp/geom/phantomSide", this, "Side of the water phantom cube.", "value > 0");
  fDepthCmd = MakeLengthCommand("/phsp/geom/depth", this,
                                "Depth of the scoring-box centre below the phantom's -z (entrance) face.",
                                "value > 0");
  fBoxSideCmd = MakeLengthCommand("/phsp/geom/boxSide", this, "Side of a cubic scoring box.", "value > 0");
  fShellCmd = MakeLengthCommand("/phsp/geom/shell", this,
                                "Thickness of DNA-physics water on each side of the box (shell cube side = box + 2 x shell).",
                                "value >= 0");
  fLatticePitchCmd = MakeLengthCommand("/phsp/geom/latticePitch", this,
                                       "Scoring-lattice pitch: 0 = a single box on the axis, else > boxSide + 2 x shell.",
                                       "value >= 0");
}

RecordMessenger::~RecordMessenger() = default;

void RecordMessenger::SetNewValue(G4UIcommand* command, G4String value)
{
  const G4double length = G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value);
  if (command == fPhantomSideCmd.get())
    fDetector->SetPhantomSide(length);
  else if (command == fDepthCmd.get())
    fDetector->SetDepth(length);
  else if (command == fBoxSideCmd.get())
    fDetector->SetBoxSide(length);
  else if (command == fShellCmd.get())
    fDetector->SetShell(length);
  else if (command == fLatticePitchCmd.get())
    fDetector->SetLatticePitch(length);
}

}  // namespace phsp
