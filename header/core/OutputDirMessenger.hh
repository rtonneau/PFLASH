/// \file OutputDirMessenger.hh
/// \brief PreInit macro control of the output directory.
/// Requires Geant4 UI runtime; not standard-library-only. Command ownership
/// and callbacks follow the UI lifecycle; callers must serialize access.

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
/// Owns /run/outputDir; configuration failures raise fatal G4Exception.
class OutputDirMessenger : public G4UImessenger
{
public:
    /// Register the command; construct while the Geant4 UI is available.
    OutputDirMessenger();
    /// Delete the owned command; no concurrent callbacks may be active.
    virtual ~OutputDirMessenger();

    /// Apply newValue as a directory path for the owned command; ignore others.
    /// Failed configuration raises FatalException with the returned error.
    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    /// Return empty for any command; the UI exposes no current path value.
    virtual G4String GetCurrentValue(G4UIcommand *command);

private:
    G4UIcmdWithAString *fpDirCmd;
};

#endif // OUTPUT_DIR_MESSENGER_HH
