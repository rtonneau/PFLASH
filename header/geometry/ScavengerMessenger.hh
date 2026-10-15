/// \file ScavengerMessenger.hh
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

class ScavengerMessenger : public G4UImessenger
{
public:
  explicit ScavengerMessenger(DnaChemistryWorld* world);
  ~ScavengerMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
  DnaChemistryWorld* fpWorld;
  G4UIcommand* fpScavengerCmd;
};

#endif  // ScavengerMessenger_h
