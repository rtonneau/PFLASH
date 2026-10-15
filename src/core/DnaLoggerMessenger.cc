#include "core/DnaLoggerMessenger.hh"
#include "core/DnaLogger.hh"

#include "G4UIcmdWithAString.hh"

DnaLoggerMessenger::DnaLoggerMessenger()
    : G4UImessenger()
{
    fpLevelCmd = new G4UIcmdWithAString("/dnaLogger/verbose", this);
    fpLevelCmd->SetGuidance("Set DnaLogger verbosity level.");
    fpLevelCmd->SetParameterName("level", false);
    fpLevelCmd->SetCandidates(DnaLogger::LevelCandidates());
    fpLevelCmd->AvailableForStates(G4State_PreInit, G4State_Idle, G4State_GeomClosed, G4State_EventProc);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

DnaLoggerMessenger::~DnaLoggerMessenger()
{
    delete fpLevelCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void DnaLoggerMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    if (command == fpLevelCmd)
    {
        DnaLogger::SetLevel(DnaLogger::LevelFromString(newValue));
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4String DnaLoggerMessenger::GetCurrentValue(G4UIcommand *command)
{
    if (command == fpLevelCmd)
    {
        return DnaLogger::LevelToString(DnaLogger::GetLevel());
    }
    return "";
}
