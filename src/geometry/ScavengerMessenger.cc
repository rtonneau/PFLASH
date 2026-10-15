/// \file ScavengerMessenger.cc
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
  // All three are strings so ScavengerSpec reports every malformed value as a
  // fatal error, instead of the UI rejecting a bad number non-fatally.
  fpScavengerCmd->SetParameter(new G4UIparameter("species", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("value", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("unit", 's', false));
  fpScavengerCmd->AvailableForStates(G4State_PreInit);
  // The world lives on the master only (as in the UHDR example).
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
  std::string err;
  if (!ScavengerSpec::Parse(newValue, entry, err)) {
    G4Exception("ScavengerMessenger::SetNewValue", "InvalidScavenger", FatalException,
                err.c_str());
    return;
  }
  fpWorld->SetScavenger(entry);
}
