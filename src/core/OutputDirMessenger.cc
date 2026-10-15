#include "core/OutputDirMessenger.hh"
#include "core/OutputDir.hh"

#include "G4UIcmdWithAString.hh"

OutputDirMessenger::OutputDirMessenger()
    : G4UImessenger()
{
    fpDirCmd = new G4UIcmdWithAString("/run/outputDir", this);
    fpDirCmd->SetGuidance(
        "Set the output directory for all output files (Species.Txt, the CSV "
        "ntuples, per-thread/per-event pre-chemical dumps, and the "
        "/chem/reaction/dump target), same as the --dir command-line flag. "
        "Fatal if it conflicts with a value already set (by --dir or an "
        "earlier /run/outputDir) -- use only one, with matching values.");
    fpDirCmd->SetParameterName("path", false);
    fpDirCmd->AvailableForStates(G4State_PreInit);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

OutputDirMessenger::~OutputDirMessenger()
{
    delete fpDirCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void OutputDirMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    if (command == fpDirCmd)
    {
        G4String err;
        if (!OutputDir::ConfigureFromMacro(newValue, err))
        {
            G4Exception("OutputDirMessenger::SetNewValue", "ConflictingOutputDir",
                        FatalException, err.c_str());
        }
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4String OutputDirMessenger::GetCurrentValue(G4UIcommand * /*command*/)
{
    return "";
}
