/// \file ScavengerMessenger.cc
/// Parses the PreInit bulk-scavenger command into the shared chemistry world.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Configure on the master before worker reads; follow Geant4 initialization callbacks.

#include "geometry/ScavengerMessenger.hh"

#include "geometry/DnaChemistryWorld.hh"
#include "geometry/ScavengerSpec.hh"

#include "G4ApplicationState.hh"
#include "G4UIcommand.hh"
#include "G4UIparameter.hh"

#include <string>

ScavengerMessenger::ScavengerMessenger(DnaChemistryWorld* world) : fpWorld(world)
{
  fpScavengerCmd = new G4UIcommand("/chem/env/scavenger", this);
  fpScavengerCmd->SetGuidance(
    "Set an exogenous dissolved species as a bulk scavenger: <species> <value> <unit>.");
  fpScavengerCmd->SetGuidance(
    "unit: M, mM, uM, or % (O2 only: % of a pure-O2 atmosphere, kH = 0.0013 M).");
  fpScavengerCmd->SetGuidance(
    "Repeating a species replaces its value; 0 means absent. The reactions come from the "
    "selected Chemistry (bulk reactions). Must be issued before /run/initialize.");
  // String parameters let ScavengerSpec diagnose invalid numeric values.
  // Missing required tokens can still be rejected by Geant4 before this callback.
  fpScavengerCmd->SetParameter(new G4UIparameter("species", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("value", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("unit", 's', false));
  fpScavengerCmd->AvailableForStates(G4State_PreInit);
  // Configure the shared world on the master before workers read its composition.
  fpScavengerCmd->SetToBeBroadcasted(false);
}

ScavengerMessenger::~ScavengerMessenger()
{
  delete fpScavengerCmd;
}

void ScavengerMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command != fpScavengerCmd) {
    return;
  }
  ScavengerSpec::Entry entry;
  std::string error;
  if (!ScavengerSpec::Parse(newValue, entry, error)) {
    G4Exception("ScavengerMessenger::SetNewValue", "InvalidScavenger", FatalException,
                error.c_str());
    return;
  }
  fpWorld->SetScavenger(entry);
}
