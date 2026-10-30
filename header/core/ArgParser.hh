/// \file ArgParser.hh
/// \brief General-purpose CLI flag registration and parsing.
///
/// Uses Geant4 types but no kernel calls; not standard-library-only.
/// Callers synchronize mutation of an instance. No logging or process exit.
/// Flags register with AddIntFlag /
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

/// Registered CLI flags with optional validators; errors use bool + GetError().
/// Values have no intrinsic units; callers define their meaning.
class ArgParser
{
public:
  /// Return false and fill err to reject a value; validator exceptions propagate.
  using IntValidator = std::function<bool(G4int value, G4String &err)>;
  /// Return false and fill err to reject text; validator exceptions propagate.
  using StringValidator = std::function<bool(const G4String &value, G4String &err)>;

  /// Register/replace name as a positive integer flag with an optional validator.
  void AddIntFlag(const G4String &name, IntValidator validator = nullptr);
  /// Register/replace name as a string flag with an optional validator.
  void AddStringFlag(const G4String &name, StringValidator validator = nullptr);
  /// Register/replace name as a presence flag; consumes no value token.
  void AddBoolFlag(const G4String &name);

  /// Scans argv (from index 1) for registered flags. Returns false on the
  /// first validation failure; GetError() then holds the "<flag>: <message>"
  /// text. Unregistered tokens are skipped, not errors.
  /// argv must contain argc valid argument strings. Earlier values and errors
  /// are retained across calls; failure leaves already-parsed values in place.
  /// Validators may propagate exceptions; this class raises no G4Exception.
  G4bool Parse(int argc, char **argv);

  /// Borrow the last error (initially empty); success does not clear it.
  const G4String &GetError() const { return fError; }

  /// Return the stored integer for name, or 0 when no integer is stored.
  G4int GetInt(const G4String &name) const;
  /// Return the stored string for name, or empty when no string is stored.
  G4String GetString(const G4String &name) const;
  /// Return the stored presence value for name, or false when absent.
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

  G4bool ParseIntValue(const G4String &token, const G4String &value,
                      const FlagSpec &spec);
  G4bool ParseStringValue(const G4String &token, const G4String &value,
                         const FlagSpec &spec);

  std::map<G4String, FlagSpec> fFlags;
  std::map<G4String, G4int> fIntValues;
  std::map<G4String, G4String> fStringValues;
  std::map<G4String, G4bool> fBoolValues;
  G4String fError;
};

#endif // ArgParser_h
