/// \file MacroResolver.cc
/// \brief See MacroResolver.hh.

#include "core/MacroResolver.hh"

#include <filesystem>
#include <system_error>

namespace MacroResolver
{
namespace
{
bool IsFile(const std::string& p)
{
  std::error_code ec;
  return std::filesystem::is_regular_file(std::filesystem::path(p), ec);
}
}  // namespace

Result Resolve(const std::string& arg, const std::string& exeDir)
{
  namespace fs = std::filesystem;
  Result r;
  r.tried.push_back(arg);
  r.tried.push_back((fs::path(exeDir) / arg).string());
  r.tried.push_back((fs::path(exeDir) / "macro" / arg).string());
  for (const auto& c : r.tried) {
    if (IsFile(c)) {
      r.found = true;
      r.path = c;
      return r;
    }
  }
  return r;
}
}  // namespace MacroResolver
