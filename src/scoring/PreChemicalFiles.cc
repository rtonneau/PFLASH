/// \file PreChemicalFiles.cc
/// \brief Name, discover and publish staged pre-chemical files.
/// Standard library only.
/// Callers coordinate filesystem changes and resolver access.
///
/// Portable: standard library only. See the ADR 0005 addendum
/// (docs/adr/0005-manifest-per-dump.md).

#include "scoring/PreChemicalFiles.hh"

#include <algorithm>
#include <filesystem>
#include <tuple>

namespace fs = std::filesystem;

namespace
{
  const std::string kPrefix = "PreChemical_run";
  const std::string kMiddle = "_event";
  const std::string kSuffix = ".txt";

  struct Entry
  {
    int run;
    int event;
    std::string name;
  };
  void MoveEntries(const std::vector<Entry>& entries, const std::string& stagingDir,
                   const std::function<std::string(const std::string&)>& resolveTarget,
                   PreChemicalFiles::MoveResult& result)
  {
    for (const Entry& entry : entries) {
      const fs::path source = fs::path(stagingDir) / entry.name;
      const fs::path target = resolveTarget(entry.name);

      std::error_code moveEc;
      if (target.has_parent_path()) fs::create_directories(target.parent_path(), moveEc);
      if (!moveEc) fs::remove(target, moveEc);
      if (!moveEc) fs::rename(source, target, moveEc);
      if (moveEc) {
        result.failures.push_back("Cannot move '" + source.string() + "' to '" + target.string() +
                                  "': " + moveEc.message());
        continue;
      }
      result.moved.push_back(target.filename().string());
    }
  }

  // Nine decimal digits fit in int on the supported platforms.
  constexpr int kMaxIdentifierDigits = 9;
  bool ParseDigits(const std::string& digits, int& value)
  {
    if (digits.empty() || digits.size() > kMaxIdentifierDigits) return false;
    long parsedValue = 0;
    for (char digit : digits) {
      if (digit < '0' || digit > '9') return false;
      parsedValue = parsedValue * 10 + (digit - '0');
    }
    value = static_cast<int>(parsedValue);
    return true;
  }
}

std::string PreChemicalFiles::StagedFileName(int runId, int eventId)
{
  return kPrefix + std::to_string(runId) + kMiddle + std::to_string(eventId) + kSuffix;
}

bool PreChemicalFiles::ParseStagedFileName(const std::string& name, int& runId, int& eventId)
{
  if (name.size() <= kPrefix.size() + kMiddle.size() + kSuffix.size()) return false;
  if (name.compare(0, kPrefix.size(), kPrefix) != 0) return false;
  if (name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) return false;

  const std::string body =
    name.substr(kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
  const auto pos = body.find(kMiddle);
  if (pos == std::string::npos) return false;

  int parsedRunId = 0;
  int parsedEventId = 0;
  if (!ParseDigits(body.substr(0, pos), parsedRunId)) return false;
  if (!ParseDigits(body.substr(pos + kMiddle.size()), parsedEventId)) return false;
  runId = parsedRunId;
  eventId = parsedEventId;
  return true;
}

std::string PreChemicalFiles::StagingDir(const std::string& outputDir)
{
  if (outputDir.empty()) return ".pending_prechem";
  return outputDir + "/.pending_prechem";
}

bool PreChemicalFiles::EnsureStagingDir(const std::string& outputDir, std::string& err)
{
  const std::string dir = StagingDir(outputDir);
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    err = "Cannot create staging folder '" + dir + "': " + ec.message();
    return false;
  }
  if (!fs::is_directory(dir, ec)) {
    err = "Staging path '" + dir + "' is not a directory";
    return false;
  }
  return true;
}

PreChemicalFiles::MoveResult PreChemicalFiles::MoveStaged(
  const std::string& stagingDir,
  const std::function<std::string(const std::string&)>& resolveTarget)
{
  MoveResult result;

  std::error_code ec;
  if (!fs::is_directory(stagingDir, ec)) return result;

  std::vector<Entry> entries;

  for (fs::directory_iterator it(stagingDir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code fileEc;
    if (!it->is_regular_file(fileEc)) continue;
    const std::string name = it->path().filename().string();
    int parsedRunId = 0;
    int parsedEventId = 0;
    if (!ParseStagedFileName(name, parsedRunId, parsedEventId)) continue;
    entries.push_back({parsedRunId, parsedEventId, name});
  }
  if (ec) {
    result.failures.push_back("Cannot scan staging folder '" + stagingDir + "': " + ec.message());
  }

  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    return std::tie(a.run, a.event) < std::tie(b.run, b.event);
  });

  MoveEntries(entries, stagingDir, resolveTarget, result);
  return result;
}
