#ifndef DNA_LOGGER_MESSENGER_HH
#define DNA_LOGGER_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file DnaLoggerMessenger.hh
    UI command to control DnaLogger verbosity from a macro file:
    /dnaLogger/verbose [Quiet|Error|Warning|Info|Debug|Trace]
*/
class DnaLoggerMessenger : public G4UImessenger
{
public:
    DnaLoggerMessenger();
    virtual ~DnaLoggerMessenger();

    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    virtual G4String GetCurrentValue(G4UIcommand *command);

private:
    G4UIcmdWithAString *fpLevelCmd;
};

#endif // DNA_LOGGER_MESSENGER_HH
