/// \file ScavengerMessenger.hh
/// Parses the PreInit bulk-scavenger command into the shared chemistry world.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Configure on the master before worker reads; follow Geant4 initialization callbacks.

/// \brief /chem/env/scavenger <species> <value> <unit> (PreInit only).
///
/// A plain G4UImessenger: the command takes three tokens, which
/// G4GenericMessenger's method dispatch cannot pass through (it cuts a
/// string argument to its first token). Attaches to the /chem/env/ directory
/// that DnaChemistryWorld's G4GenericMessenger creates, so construct it after
/// that one. Parsing and checks: ScavengerSpec.
#ifndef ScavengerMessenger_h
#define ScavengerMessenger_h 1

#include "G4UImessenger.hh"

class DnaChemistryWorld;
class G4UIcommand;

/// Parses the PreInit bulk-scavenger command into the shared chemistry world.
class ScavengerMessenger : public G4UImessenger
{
public:
  /// Borrows non-null world, which must outlive this messenger. Create after /chem/env/.
  /// Registers a master-only PreInit command; Geant4/allocation failures propagate.
  explicit ScavengerMessenger(DnaChemistryWorld* world);
  /// Deletes the owned UI command after dispatch stops.
  ~ScavengerMessenger() override;

  /// Handles its own command, ignoring other pointers. Expects species/value/unit tokens.
  /// Parse failure raises FatalException; successful concentrations are stored in mol/L.
  void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
  DnaChemistryWorld* fpWorld;
  G4UIcommand* fpScavengerCmd;
};

#endif  // ScavengerMessenger_h
