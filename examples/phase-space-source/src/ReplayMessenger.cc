#include "ReplayMessenger.hh"

#include <cstdint>
#include <exception>
#include <string>

#include "PhaseSpaceSource.hh"
#include "ReplayChemistryList.hh"

#include "G4Exception.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcommand.hh"
#include "G4UIdirectory.hh"
#include "G4UIparameter.hh"

namespace phsp
{

namespace
{

std::string Trim(const std::string& text)
{
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

// An unsigned 64-bit decimal integer; FatalException otherwise.
std::uint64_t ParseUnsigned(const char* command, const std::string& text)
{
  const std::string value = Trim(text);
  std::uint64_t result = 0;
  bool ok = !value.empty() && value.find_first_not_of("0123456789") == std::string::npos;
  if (ok)
  {
    try
    {
      result = std::stoull(value, nullptr, 10);
    }
    catch (const std::exception&)
    {
      ok = false;
    }
  }
  if (!ok)
  {
    const std::string message = std::string(command) + " needs an unsigned 64-bit decimal integer, got '" + value + "'";
    G4Exception("ReplayMessenger", "PhspReplay020", FatalErrorInArgument, message.c_str());
  }
  return result;
}

std::unique_ptr<G4UIcmdWithAString> MakeStringCommand(const char* path, G4UImessenger* messenger,
                                                      const char* guidance, const char* parameter)
{
  auto command = std::make_unique<G4UIcmdWithAString>(path, messenger);
  command->SetGuidance(guidance);
  command->SetParameterName(parameter, false);
  command->SetToBeBroadcasted(false);
  return command;
}

}  // namespace

ReplayMessenger::ReplayMessenger(ReplaySource& source, ReplayChemistrySettings& chemistry)
  : fSource(source), fChemistry(chemistry)
{
  fPhspDirectory = std::make_unique<G4UIdirectory>("/phsp/");
  fPhspDirectory->SetGuidance("Phase-space source example (phsp-replay).");
  fSourceDirectory = std::make_unique<G4UIdirectory>("/phsp/source/");
  fSourceDirectory->SetGuidance("Stage-2 phase-space source: files, range and base seed.");
  fGeomDirectory = std::make_unique<G4UIdirectory>("/phsp/geom/");
  fGeomDirectory->SetGuidance("Stage-2 geometry (the box side comes from the files).");

  fAddFileCmd = MakeStringCommand("/phsp/source/addFile", this,
                                  "Append a phase-space file (.phsp) to the ordered list. The global history index "
                                  "runs over file order, then history index; a file ID given twice is fatal.",
                                  "path");
  fAddFileCmd->AvailableForStates(G4State_PreInit);

  fFirstCmd = MakeStringCommand("/phsp/source/first", this,
                                "Global index of the first history of the next /run/beamOn N, which replays "
                                "histories first..first+N-1 (past the end is fatal).",
                                "first");
  fFirstCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fBaseSeedCmd = MakeStringCommand("/phsp/source/baseSeed", this,
                                   "Stage-2 base seed (unsigned 64-bit). Each history is replayed with the seed "
                                   "HistorySeed(baseSeed, fileId, historyIndex) (ADR 0015).",
                                   "seed");
  fBaseSeedCmd->AvailableForStates(G4State_PreInit);

  fBoxSideCmd = std::make_unique<G4UIcmdWithADoubleAndUnit>("/phsp/geom/boxSide", this);
  fBoxSideCmd->SetGuidance("Optional: the expected box side. It must equal the box side of the phase-space files "
                           "(fatal at /run/initialize otherwise); the box is built from the files.");
  fBoxSideCmd->SetParameterName("value", false);
  fBoxSideCmd->SetRange("value > 0");
  fBoxSideCmd->SetUnitCategory("Length");
  fBoxSideCmd->AvailableForStates(G4State_PreInit);
  fBoxSideCmd->SetToBeBroadcasted(false);

  fChemMarginCmd = std::make_unique<G4UIcmdWithADoubleAndUnit>("/phsp/geom/chemMargin", this);
  fChemMarginCmd->SetGuidance("Water margin around the box (default 2 um, >= 0): the molecules diffuse in box + "
                              "margin, and G4DNABrownianTransportation kills a molecule outside water. Physics "
                              "tracks are killed when they leave the box and never step in it.");
  fChemMarginCmd->SetParameterName("value", false);
  fChemMarginCmd->SetRange("value >= 0");
  fChemMarginCmd->SetUnitCategory("Length");
  fChemMarginCmd->AvailableForStates(G4State_PreInit);
  fChemMarginCmd->SetToBeBroadcasted(false);

  fChemDirectory = std::make_unique<G4UIdirectory>("/phsp/chem/");
  fChemDirectory->SetGuidance("Stage-2 chemistry: PureWater network, IRT_syn, pH buffer, dissolved O2. "
                              "The end time is /scheduler/endTime after /run/initialize (default 1 us).");

  fChemEnableCmd = std::make_unique<G4UIcmdWithABool>("/phsp/chem/enable", this);
  fChemEnableCmd->SetGuidance("Stage-2 chemistry on (default) or off. Off: physics only, no molecule is "
                              "created and the output is that of the physics-only replay.");
  fChemEnableCmd->SetParameterName("enable", false);
  fChemEnableCmd->AvailableForStates(G4State_PreInit);
  fChemEnableCmd->SetToBeBroadcasted(false);

  fPhCmd = std::make_unique<G4UIcmdWithADouble>("/phsp/chem/pH", this);
  fPhCmd->SetGuidance("pH of the bulk acid-base buffer (H3Op(B), OHm(B)), in (0, 14); default 7.");
  fPhCmd->SetParameterName("pH", false);
  fPhCmd->AvailableForStates(G4State_PreInit);
  fPhCmd->SetToBeBroadcasted(false);

  fO2Cmd = std::make_unique<G4UIcommand>("/phsp/chem/o2", this);
  fO2Cmd->SetGuidance("Dissolved O2 (bulk scavenger of e_aq, H and O-): <value> <unit>, unit M, mM, uM or % "
                      "(of O2 saturation, kH = 1.3 mM: 21 % = 0.273 mM). Default 0 (absent).");
  fO2Cmd->SetParameter(new G4UIparameter("value", 'd', false));
  fO2Cmd->SetParameter(new G4UIparameter("unit", 's', false));
  fO2Cmd->AvailableForStates(G4State_PreInit);
  fO2Cmd->SetToBeBroadcasted(false);
}

ReplayMessenger::~ReplayMessenger() = default;

void ReplayMessenger::SetNewValue(G4UIcommand* command, G4String value)
{
  if (command == fAddFileCmd.get())
  {
    std::string path = Trim(value);
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2);
    fSource.AddFile(path);
  }
  else if (command == fFirstCmd.get())
    fSource.SetFirst(ParseUnsigned("/phsp/source/first", value));
  else if (command == fBaseSeedCmd.get())
    fSource.SetBaseSeed(ParseUnsigned("/phsp/source/baseSeed", value));
  else if (command == fBoxSideCmd.get())
    fSource.SetRequestedBoxSide(G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value));
  else if (command == fChemMarginCmd.get())
    fChemistry.chemMargin = G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value);
  else if (command == fChemEnableCmd.get())
    fChemistry.enabled = G4UIcmdWithABool::GetNewBoolValue(value);
  else if (command == fPhCmd.get())
  {
    const G4double pH = G4UIcmdWithADouble::GetNewDoubleValue(value);
    if (!(pH > 0 && pH < 14))
    {
      const std::string message = "/phsp/chem/pH must be in (0, 14), got " + Trim(value);
      G4Exception("ReplayMessenger", "PhspReplay021", FatalErrorInArgument, message.c_str());
      return;
    }
    fChemistry.pH = pH;
  }
  else if (command == fO2Cmd.get())
  {
    double molarity = 0;
    std::string error;
    if (!ParseO2Concentration(value, molarity, error))
    {
      const std::string message = "/phsp/chem/o2: " + error;
      G4Exception("ReplayMessenger", "PhspReplay022", FatalErrorInArgument, message.c_str());
      return;
    }
    fChemistry.o2Molarity = molarity;
    fChemistry.o2Text = Trim(value);
  }
}

}  // namespace phsp
