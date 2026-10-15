/// \file MacroResolver.hh
/// \brief Locates a macro file given on the command line.
///
/// Pure, standard library only (portable like ArgParser; adjust the include
/// path prefix on copy). No logging, no process exit.

#ifndef MacroResolver_h
#define MacroResolver_h 1

#include <string>
#include <vector>

namespace MacroResolver
{
struct Result
{
  bool found = false;
  std::string path;                // the first existing candidate (when found)
  std::vector<std::string> tried;  // the three candidates, in order
};

/// Returns the first existing of: arg as given, <exeDir>/<arg>,
/// <exeDir>/macro/<arg>. When none exists, found is false and tried lists
/// the three paths for the error message.
Result Resolve(const std::string& arg, const std::string& exeDir);
}  // namespace MacroResolver

#endif
