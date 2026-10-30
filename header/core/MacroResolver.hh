/// \file MacroResolver.hh
/// \brief Locates a macro file given on the command line.
///
/// Portable, standard library only (adjust the include path prefix on copy).
/// Stateless: concurrent calls share no mutable state. Filesystem contents
/// can change during lookup. No logging or process exit.
///

#ifndef MacroResolver_h
#define MacroResolver_h 1

#include <string>
#include <vector>

namespace MacroResolver
{
/// Lookup outcome; path is empty on failure, tried holds all three candidates.
struct Result
{
  bool found = false;
  std::string path;                // the first existing candidate (when found)
  std::vector<std::string> tried;  // the three candidates, in order
};

/// Returns the first existing of: arg as given, <exeDir>/<arg>,
/// <exeDir>/macro/<arg>. When none exists, found is false and tried lists
/// the three paths for the error message.
/// arg is the requested path and exeDir is the executable directory.
/// Candidates must be regular files; filesystem query errors count as misses.
/// No G4Exception; allocation/path-conversion exceptions can propagate.
Result Resolve(const std::string& arg, const std::string& exeDir);
}  // namespace MacroResolver

#endif
