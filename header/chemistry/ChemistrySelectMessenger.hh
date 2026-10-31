/// \file ChemistrySelectMessenger.hh
/// Exposes Chemistry selection and listing commands.
/// Dependencies: Geant4 UI and project registry; not standard-library-only.
/// Thread safety: Create and dispatch on the master; selection changes are restricted to PreInit.
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

/// Owns the UI commands described above; use on the master with the Geant4 UI active.
class ChemistrySelectMessenger : public G4UImessenger
{
public:
  /// Registers commands after G4DNAChemistryManager creates /chem/.
  ChemistrySelectMessenger();
  /// Deletes owned commands; the UI must no longer dispatch to this instance.
  ~ChemistrySelectMessenger() override;

  /// Selects the name or prints available names. Unknown/different selection raises
  /// FatalException; a selection hook adds Extra molecules on the master in PreInit.
  /// Unknown command pointers do nothing.
  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  /// Returns the owned command's current value; unknown command pointers return empty.
  G4String GetCurrentValue(G4UIcommand* command) override;

private:
  G4UIcmdWithAString* fpSelectCmd;
  G4UIcmdWithoutParameter* fpListCmd;
};

#endif  // ChemistrySelectMessenger_h
