#include "core/DnaLogger.hh"

#include "G4ios.hh"

#include <atomic>

namespace
{
    std::atomic<G4int> gVerbosityLevel{static_cast<G4int>(DnaLogger::Level::Quiet)};
}

namespace DnaLogger
{
    Level GetLevel()
    {
        return static_cast<Level>(gVerbosityLevel.load(std::memory_order_relaxed));
    }

    void SetLevel(Level level)
    {
        gVerbosityLevel.store(static_cast<G4int>(level), std::memory_order_relaxed);
    }

    G4bool Enabled(Level level)
    {
        return static_cast<G4int>(level) <= gVerbosityLevel.load(std::memory_order_relaxed);
    }

    void Print(Level level, const G4String &message)
    {
        if (Enabled(level))
        {
            G4cout << message << G4endl;
        }
    }

    const char *LevelCandidates()
    {
        return "Quiet Error Warning Info Debug Trace";
    }

    G4String LevelToString(Level level)
    {
        switch (level)
        {
        case Level::Quiet:
            return "Quiet";
        case Level::Error:
            return "Error";
        case Level::Warning:
            return "Warning";
        case Level::Info:
            return "Info";
        case Level::Debug:
            return "Debug";
        case Level::Trace:
            return "Trace";
        }
        return "Quiet";
    }

    Level LevelFromString(const G4String &name)
    {
        G4String lower = name;
        lower.toLower();

        if (lower == "quiet")
            return Level::Quiet;
        if (lower == "error")
            return Level::Error;
        if (lower == "warning")
            return Level::Warning;
        if (lower == "info")
            return Level::Info;
        if (lower == "debug")
            return Level::Debug;
        if (lower == "trace")
            return Level::Trace;

        // Should not happen: G4UIcmdWithAString restricts input via SetCandidates()
        G4cerr << "[DnaLogger] Unknown level '" << name << "', defaulting to Quiet" << G4endl;
        return Level::Quiet;
    }
}
