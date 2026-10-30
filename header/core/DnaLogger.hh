/// \file DnaLogger.hh
/// \brief Application log levels and Geant4 stream output.
/// Geant4-dependent, not standard-library-only. Level access is atomic;
/// Geant4 destinations determine output synchronization and ordering.

#ifndef DNA_LOGGER_HH
#define DNA_LOGGER_HH 1

#include "globals.hh"

#include <atomic>

namespace DnaLogger
{
    /// Increasing verbosity thresholds; levels have no physical units.
    enum class Level : G4int
    {
        Quiet = 0,
        Error = 1,
        Warning = 2,
        Info = 3,
        Debug = 4,
        Trace = 5
    };

    /// Return the process-wide verbosity (initially Quiet).
    Level GetLevel();
    /// Store level as the verbosity threshold; no validation or G4Exception.
    void SetLevel(Level level);
    /// Return whether level is at or below the current verbosity threshold.
    G4bool Enabled(Level level);
    /// Write message and a newline to G4cout when Enabled(level); no error result.
    void Print(Level level, const G4String &message);

    /// Borrow the static, space-separated list of names accepted by the UI command.
    const char *LevelCandidates();
    /// Return the name of level; unknown enum values map to Quiet.
    G4String LevelToString(Level level);
    /// Parse name case-insensitively; unknown names emit G4cerr and return Quiet.
    Level LevelFromString(const G4String &name);
}

#endif // DNA_LOGGER_HH
