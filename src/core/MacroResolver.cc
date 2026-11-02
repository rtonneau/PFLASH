/// \file MacroResolver.cc
/// \brief Ordered macro-file lookup; portable, standard library only.
/// Stateless and safe for concurrent calls; filesystem changes are external.

#include "core/MacroResolver.hh"

#include <filesystem>
#include <system_error>

namespace MacroResolver
{
namespace
{
bool IsFile(const std::string& path)
{
  std::error_code ec;
  return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}
}  // namespace

Result Resolve(const std::string& arg, const std::string& exeDir)
{
  namespace fs = std::filesystem;
  Result result;
  result.tried.push_back(arg);
  result.tried.push_back((fs::path(exeDir) / arg).string());
  result.tried.push_back((fs::path(exeDir) / "macro" / arg).string());
  for (const auto& candidatePath : result.tried) {
    if (IsFile(candidatePath)) {
      result.found = true;
      result.path = candidatePath;
      return result;
    }
  }
  return result;
}
}  // namespace MacroResolver
