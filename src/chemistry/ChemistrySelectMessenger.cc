/// \file ChemistrySelectMessenger.cc
#include "chemistry/ChemistrySelectMessenger.hh"

#include "chemistry/ChemistryRegistry.hh"

#include "G4ApplicationState.hh"
#include "G4Threading.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"

#include <string>

ChemistrySelectMessenger::ChemistrySelectMessenger()
{
  fpSelectCmd = new G4UIcmdWithAString("/chem/select", this);
  fpSelectCmd->SetGuidance(
    "Select the Chemistry (reaction network) for this run, by name, case-insensitive. "
    "Use /chem/list to see the names. Default: PureWater. Must be issued before "
    "/run/initialize. Issuing a different name twice, or an unknown name, is fatal.");
  fpSelectCmd->SetParameterName("name", false);
  fpSelectCmd->AvailableForStates(G4State_PreInit);

  fpListCmd = new G4UIcmdWithoutParameter("/chem/list", this);
  fpListCmd->SetGuidance("Print the available Chemistries; the default is marked.");
  fpListCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

ChemistrySelectMessenger::~ChemistrySelectMessenger()
{
  delete fpSelectCmd;
  delete fpListCmd;
}

void ChemistrySelectMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command == fpSelectCmd) {
    std::string err;
    if (!ChemistryRegistry::Select(newValue, err)) {
      G4Exception("ChemistrySelectMessenger::SetNewValue", "InvalidChemistrySelection",
                  FatalException, err.c_str());
    }
    // Molecules of the selected Chemistry (ADR 0007). The shared molecule set
    // is built by DnaChemistryList::ConstructMolecule when the physics list is
    // handed to the run manager, before any macro command, so a Chemistry
    // selected here adds its own molecules now: still PreInit, on the master,
    // before /run/initialize gives every particle a process manager. The hook
    // is idempotent, so selecting the same name again is harmless.
    const auto* selected = ChemistryRegistry::Selected();
    if (selected != nullptr && selected->constructMolecules != nullptr &&
        !G4Threading::IsWorkerThread()) {
      selected->constructMolecules();
    }
  }
  else if (command == fpListCmd) {
    G4cout << "Available chemistries:";
    for (const auto& name : ChemistryRegistry::Names()) {
      G4cout << " " << name << (name == ChemistryRegistry::kDefaultName ? " (default)" : "");
    }
    G4cout << G4endl;
  }
}

G4String ChemistrySelectMessenger::GetCurrentValue(G4UIcommand* command)
{
  if (command == fpSelectCmd) {
    const auto* selected = ChemistryRegistry::Selected();
    return selected != nullptr ? G4String(selected->name) : G4String("");
  }
  return "";
}
