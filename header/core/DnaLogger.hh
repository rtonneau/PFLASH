#ifndef DNA_LOGGER_HH
#define DNA_LOGGER_HH 1

#include "globals.hh"

#include <atomic>

namespace DnaLogger
{
    enum class Level : G4int
    {
        Quiet = 0,
        Error = 1,
        Warning = 2,
        Info = 3,
        Debug = 4,
        Trace = 5
    };

    Level GetLevel();
    void SetLevel(Level level);
    G4bool Enabled(Level level);
    void Print(Level level, const G4String &message);

    /** Candidate names accepted by the UI command, e.g. "Quiet Error Warning Info Debug Trace" */
    const char *LevelCandidates();
    G4String LevelToString(Level level);
    Level LevelFromString(const G4String &name);
}

#endif // DNA_LOGGER_HH
