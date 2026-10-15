/// \file ArgParser.hh
/// \brief General-purpose CLI flag registration and parsing.
///
/// Pure: no logging, no process exit. Flags register with AddIntFlag /
/// AddStringFlag / AddBoolFlag (each Int/String flag taking an optional
/// validator for flag-specific rules), then Parse(argc, argv) fills
/// everything in. Tokens that aren't a registered flag name are ignored
/// (e.g. sim.cc's argv[1] macro-file convention). On failure, GetError()
/// returns one message already prefixed with the flag name.

#ifndef ArgParser_h
#define ArgParser_h 1

#include "globals.hh"

#include <functional>
#include <map>

class ArgParser
{
public:
  using IntValidator = std::function<bool(G4int value, G4String &err)>;
  using StringValidator = std::function<bool(const G4String &value, G4String &err)>;

  void AddIntFlag(const G4String &name, IntValidator validator = nullptr);
  void AddStringFlag(const G4String &name, StringValidator validator = nullptr);
  void AddBoolFlag(const G4String &name);

  /// Scans argv (from index 1) for registered flags. Returns false on the
  /// first validation failure; GetError() then holds the "<flag>: <message>"
  /// text. Unregistered tokens are skipped, not errors.
  G4bool Parse(int argc, char **argv);

  const G4String &GetError() const { return fError; }

  /// Returns the parsed value, or 0/""/false if that flag was not present.
  G4int GetInt(const G4String &name) const;
  G4String GetString(const G4String &name) const;
  G4bool GetBool(const G4String &name) const;

private:
  enum class FlagType
  {
    Int,
    String,
    Bool
  };

  struct FlagSpec
  {
    FlagType type;
    IntValidator intValidator;
    StringValidator stringValidator;
  };

  std::map<G4String, FlagSpec> fFlags;
  std::map<G4String, G4int> fIntValues;
  std::map<G4String, G4String> fStringValues;
  std::map<G4String, G4bool> fBoolValues;
  G4String fError;
};

#endif // ArgParser_h
