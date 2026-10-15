#ifndef OUTPUT_DIR_MESSENGER_HH
#define OUTPUT_DIR_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file OutputDirMessenger.hh
    UI command to set the process-wide output directory from a macro file:
    /run/outputDir <path>

    A second way to configure the same output directory as the --dir CLI
    flag (see OutputDir.hh). PreInit only, so it is always issued before
    /run/initialize spawns any worker thread. Setting a path that conflicts
    with an already-configured directory (from --dir or an earlier
    /run/outputDir) is a fatal configuration error.
*/
class OutputDirMessenger : public G4UImessenger
{
public:
    OutputDirMessenger();
    virtual ~OutputDirMessenger();

    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    virtual G4String GetCurrentValue(G4UIcommand *command);

private:
    G4UIcmdWithAString *fpDirCmd;
};

#endif // OUTPUT_DIR_MESSENGER_HH
