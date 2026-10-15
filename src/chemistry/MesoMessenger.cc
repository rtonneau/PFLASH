/// \file MesoMessenger.cc
#include "chemistry/MesoMessenger.hh"

#include "chemistry/MesoSettings.hh"

#include "G4ApplicationState.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAnInteger.hh"

#include <string>

namespace
{
void Fatal(const char* code, const std::string& message)
{
  G4Exception("MesoMessenger::SetNewValue", code, FatalException, message.c_str());
}
}  // namespace

MesoMessenger::MesoMessenger()
{
  fpHandOverCmd = new G4UIcmdWithADoubleAndUnit("/chem/meso/handOverTime", this);
  fpHandOverCmd->SetGuidance(
    "Global time at which the particle-based stage hands over to the mesoscopic stage. "
    "Must be > 0. Default: 5 ns. Issue before /run/initialize.");
  fpHandOverCmd->SetParameterName("handOverTime", false);
  fpHandOverCmd->SetDefaultUnit("ns");
  fpHandOverCmd->AvailableForStates(G4State_PreInit);

  fpVoxelCmd = new G4UIcmdWithADoubleAndUnit("/chem/meso/voxelSize", this);
  fpVoxelCmd->SetGuidance(
    "Target initial cell size of the mesoscopic mesh; the pixel count per side is the "
    "power of 2 giving the closest cell size (capped at 65536, see ADR 0006). Must be > 0. "
    "Default: 6.25 nm. Issue before /run/initialize.");
  fpVoxelCmd->SetParameterName("voxelSize", false);
  fpVoxelCmd->SetDefaultUnit("nm");
  fpVoxelCmd->AvailableForStates(G4State_PreInit);

  fpPerDecadeCmd = new G4UIcmdWithAnInteger("/chem/meso/timesPerDecade", this);
  fpPerDecadeCmd->SetGuidance(
    "Number of mesoscopic record times per decade (log grid from the hand-over time to the "
    "end time). Must be >= 1. Default: 10. Issue before /run/initialize.");
  fpPerDecadeCmd->SetParameterName("timesPerDecade", false);
  fpPerDecadeCmd->AvailableForStates(G4State_PreInit);

  fpSpatialOutputCmd = new G4UIcmdWithABool("/chem/meso/spatialOutput", this);
  fpSpatialOutputCmd->SetGuidance(
    "Whether to save spatial species distributions during the mesoscopic stage to an HDF5 file "
    "(SpeciesMesoSpatial.h5). Note: this generates a heavy output file. Default: false. Issue "
    "before /run/initialize.");
  fpSpatialOutputCmd->SetParameterName("spatialOutput", false);
  fpSpatialOutputCmd->AvailableForStates(G4State_PreInit);

  fpEnableCmd = new G4UIcmdWithABool("/chem/meso/enable", this);
  fpEnableCmd->SetGuidance(
    "Whether the particle-based stage hands over to the mesoscopic stage at the hand-over "
    "time. false: the particle-based stage runs to the end time. Default: true. The SBS "
    "time-step model has no mesoscopic stage: there the default means off, and an explicit "
    "'true' is fatal at /run/initialize. Issue before /run/initialize.");
  fpEnableCmd->SetParameterName("enable", false);
  fpEnableCmd->AvailableForStates(G4State_PreInit);
}

MesoMessenger::~MesoMessenger()
{
  delete fpHandOverCmd;
  delete fpVoxelCmd;
  delete fpPerDecadeCmd;
  delete fpSpatialOutputCmd;
  delete fpEnableCmd;
}

void MesoMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  auto& settings = MesoSettings::Current();
  if (command == fpHandOverCmd) {
    const G4double value = fpHandOverCmd->GetNewDoubleValue(newValue);
    if (!(value > 0.)) {
      Fatal("InvalidMesoHandOverTime",
            "/chem/meso/handOverTime must be > 0 (got " + std::to_string(value / ns) + " ns).");
    }
    settings.handOverTime = value / ns;
  }
  else if (command == fpVoxelCmd) {
    const G4double value = fpVoxelCmd->GetNewDoubleValue(newValue);
    if (!(value > 0.)) {
      Fatal("InvalidMesoVoxelSize",
            "/chem/meso/voxelSize must be > 0 (got " + std::to_string(value / nm) + " nm).");
    }
    settings.voxelSize = value / mm;
  }
  else if (command == fpPerDecadeCmd) {
    const G4int value = fpPerDecadeCmd->GetNewIntValue(newValue);
    if (value < 1) {
      Fatal("InvalidMesoTimesPerDecade",
            "/chem/meso/timesPerDecade must be >= 1 (got " + std::to_string(value) + ").");
    }
    settings.timesPerDecade = value;
  }
  else if (command == fpSpatialOutputCmd) {
    settings.spatialOutput = fpSpatialOutputCmd->GetNewBoolValue(newValue);
  }
  else if (command == fpEnableCmd) {
    // The SBS conflict is checked at /run/initialize (DnaChemistryList), when
    // the time-step model is final whatever the command order.
    settings.enabled = fpEnableCmd->GetNewBoolValue(newValue);
    settings.enabledExplicitly = true;
  }
}

G4String MesoMessenger::GetCurrentValue(G4UIcommand* command)
{
  const auto& settings = MesoSettings::Current();
  if (command == fpHandOverCmd) {
    return fpHandOverCmd->ConvertToString(settings.handOverTime * ns, "ns");
  }
  if (command == fpVoxelCmd) {
    return fpVoxelCmd->ConvertToString(settings.voxelSize * mm, "nm");
  }
  if (command == fpPerDecadeCmd) {
    return fpPerDecadeCmd->ConvertToString(settings.timesPerDecade);
  }
  if (command == fpSpatialOutputCmd) {
    return fpSpatialOutputCmd->ConvertToString(settings.spatialOutput);
  }
  if (command == fpEnableCmd) {
    return fpEnableCmd->ConvertToString(settings.enabled);
  }
  return "";
}
