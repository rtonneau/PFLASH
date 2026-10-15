/// \file ChemistrySelectMessenger.hh
/// \brief Macro commands to choose and list Chemistries.
///   /chem/select <name>   (PreInit only) pick the Chemistry for this process
///   /chem/list            print the registered Chemistries
/// /chem/ already exists (created by G4DNAChemistryManager), so this is a plain
/// G4UImessenger rather than a G4GenericMessenger, which would create a second
/// /chem/ directory. Construct it after G4DNAChemistryManager::Instance().
#ifndef ChemistrySelectMessenger_h
#define ChemistrySelectMessenger_h 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;
class G4UIcmdWithoutParameter;

class ChemistrySelectMessenger : public G4UImessenger
{
public:
  ChemistrySelectMessenger();
  ~ChemistrySelectMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  G4String GetCurrentValue(G4UIcommand* command) override;

private:
  G4UIcmdWithAString* fpSelectCmd;
  G4UIcmdWithoutParameter* fpListCmd;
};

#endif  // ChemistrySelectMessenger_h
