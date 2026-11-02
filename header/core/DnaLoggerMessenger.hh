/// \file DnaLoggerMessenger.hh
/// \brief Macro control of application log verbosity.
/// Requires Geant4 UI runtime; not standard-library-only. Command ownership
/// and callbacks follow the UI lifecycle; callers must serialize access.

#ifndef DNA_LOGGER_MESSENGER_HH
#define DNA_LOGGER_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file DnaLoggerMessenger.hh
    UI command to control DnaLogger verbosity from a macro file:
    /dnaLogger/verbose [Quiet|Error|Warning|Info|Debug|Trace]
*/
/// Owns the verbosity command; values are the DnaLogger level names.
class DnaLoggerMessenger : public G4UImessenger
{
public:
    /// Register the command; construct while the Geant4 UI is available.
    DnaLoggerMessenger();
    /// Delete the owned command; no concurrent callbacks may be active.
    virtual ~DnaLoggerMessenger();

    /// Apply newValue as a level name for the owned command; ignore others.
    /// Direct invalid names use DnaLogger's diagnostic/Quiet fallback.
    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    /// Return the level name for the owned command, otherwise empty.
    virtual G4String GetCurrentValue(G4UIcommand *command);

private:
    G4UIcmdWithAString *fpLevelCmd;
};

#endif // DNA_LOGGER_MESSENGER_HH
