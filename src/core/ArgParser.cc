/// \file ArgParser.cc
/// \brief Implementation of ArgParser

#include "core/ArgParser.hh"

#include <cstdlib>

void ArgParser::AddIntFlag(const G4String &name, IntValidator validator)
{
  fFlags[name] = FlagSpec{FlagType::Int, std::move(validator), nullptr};
}

void ArgParser::AddStringFlag(const G4String &name, StringValidator validator)
{
  fFlags[name] = FlagSpec{FlagType::String, nullptr, std::move(validator)};
}

void ArgParser::AddBoolFlag(const G4String &name)
{
  fFlags[name] = FlagSpec{FlagType::Bool, nullptr, nullptr};
}

G4bool ArgParser::Parse(int argc, char **argv)
{
  for (int i = 1; i < argc; ++i)
  {
    G4String token(argv[i]);
    auto it = fFlags.find(token);
    if (it == fFlags.end())
      continue;

    const FlagSpec &spec = it->second;

    if (spec.type == FlagType::Bool)
    {
      fBoolValues[token] = true;
      continue;
    }

    if (i + 1 >= argc)
    {
      fError = token + ": missing value";
      return false;
    }
    G4String value(argv[++i]);

    if (spec.type == FlagType::Int)
    {
      char *end = nullptr;
      long parsed = std::strtol(value.c_str(), &end, 10);
      if (end == value.c_str() || *end != '\0' || parsed <= 0)
      {
        fError = token + ": value must be a positive integer, got '" + value + "'";
        return false;
      }

      if (spec.intValidator)
      {
        G4String validatorError;
        if (!spec.intValidator(static_cast<G4int>(parsed), validatorError))
        {
          fError = token + ": " + validatorError;
          return false;
        }
      }

      fIntValues[token] = static_cast<G4int>(parsed);
    }
    else // FlagType::String
    {
      if (spec.stringValidator)
      {
        G4String validatorError;
        if (!spec.stringValidator(value, validatorError))
        {
          fError = token + ": " + validatorError;
          return false;
        }
      }

      fStringValues[token] = value;
    }
  }

  return true;
}

G4int ArgParser::GetInt(const G4String &name) const
{
  auto it = fIntValues.find(name);
  return it == fIntValues.end() ? 0 : it->second;
}

G4String ArgParser::GetString(const G4String &name) const
{
  auto it = fStringValues.find(name);
  return it == fStringValues.end() ? G4String("") : it->second;
}

G4bool ArgParser::GetBool(const G4String &name) const
{
  auto it = fBoolValues.find(name);
  return it == fBoolValues.end() ? false : it->second;
}
