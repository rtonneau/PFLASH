#include "HistoryConfig.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ptirt
{
namespace
{

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error("HistoryConfig: " + message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

std::string Trim(const std::string& text)
{
  const auto first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r");
  return text.substr(first, last - first + 1);
}

// One `key = value` line, remembered with its line number for the messages.
struct Entry
{
  std::string value;
  std::size_t line = 0;
};

// Unsigned() reads at most 10 digits.
constexpr std::uint64_t kMaxContactLimit = 9'999'999'999;

const std::set<std::string> kCommonKeys{"mode",       "libraryDir",      "endpointNs",   "chemistrySeed",
                                        "worldHalfSideUm", "halfBoxUm", "gridMinAgeNs", "gridPointsPerDecade",
                                        "newOldContactLimit"};
const std::set<std::string> kScheduleKeys{"scheduleFile"};
const std::set<std::string> kSingleKeys{"singleSource"};
const std::set<std::string> kPoissonKeys{"rateHz", "exposureS", "sourceSeed", "regionCenterUm", "regionHalfSizeUm"};

bool IsKnownKey(const std::string& key)
{
  return kCommonKeys.count(key) || kScheduleKeys.count(key) || kSingleKeys.count(key) || kPoissonKeys.count(key);
}

bool IsKeyOfMode(const std::string& key, HistoryMode mode)
{
  if (kCommonKeys.count(key)) return true;
  switch (mode)
  {
    case HistoryMode::Deterministic:
      return kScheduleKeys.count(key) > 0;
    case HistoryMode::Single:
      return kScheduleKeys.count(key) > 0 || kSingleKeys.count(key) > 0;
    case HistoryMode::Poisson:
      return kPoissonKeys.count(key) > 0;
  }
  return false;
}

// Typed access to the collected entries; every message names the line.
class Entries
{
 public:
  explicit Entries(std::map<std::string, Entry> entries) : fEntries(std::move(entries)) {}

  bool Has(const std::string& key) const { return fEntries.count(key) > 0; }

  const Entry& Get(const std::string& key) const
  {
    const auto it = fEntries.find(key);
    Require(it != fEntries.end(), "missing required key '" + key + "'");
    return it->second;
  }

  std::string Where(const std::string& key) const
  {
    return "line " + std::to_string(Get(key).line) + ": " + key + ": ";
  }

  std::vector<double> Numbers(const std::string& key) const
  {
    std::istringstream fields(Get(key).value);
    std::vector<double> values;
    for (std::string token; fields >> token;)
    {
      char* end = nullptr;
      const double value = std::strtod(token.c_str(), &end);
      Require(end != token.c_str() && *end == '\0' && std::isfinite(value),
              Where(key) + "'" + token + "' is not a finite number");
      values.push_back(value);
    }
    return values;
  }

  double Number(const std::string& key) const
  {
    const auto values = Numbers(key);
    Require(values.size() == 1, Where(key) + "expected one number");
    return values.front();
  }

  std::uint64_t Unsigned(const std::string& key, std::uint64_t minimum, std::uint64_t maximum) const
  {
    const auto& token = Get(key).value;
    const std::string range = "[" + std::to_string(minimum) + ", " + std::to_string(maximum) + "]";
    Require(!token.empty() && token.find_first_not_of("0123456789") == std::string::npos,
            Where(key) + "'" + token + "' is not an integer in " + range);
    // At most 10 digits are needed for the ranges used here; longer tokens are
    // out of range (and would overflow the conversion).
    Require(token.size() <= 10, Where(key) + token + " is outside " + range);
    const std::uint64_t value = std::strtoull(token.c_str(), nullptr, 10);
    Require(value >= minimum && value <= maximum, Where(key) + token + " is outside " + range);
    return value;
  }

  std::filesystem::path Path(const std::string& key, const std::filesystem::path& baseDirectory) const
  {
    std::filesystem::path path(Get(key).value);
    if (path.is_relative()) path = baseDirectory / path;
    return path.lexically_normal();
  }

 private:
  std::map<std::string, Entry> fEntries;
};

HistoryMode ParseMode(const Entries& entries)
{
  const auto& value = entries.Get("mode").value;
  if (value == "deterministic") return HistoryMode::Deterministic;
  if (value == "single") return HistoryMode::Single;
  if (value == "poisson") return HistoryMode::Poisson;
  Fail(entries.Where("mode") + "'" + value + "' is not one of deterministic, single, poisson");
}

}  // namespace

const char* HistoryModeName(HistoryMode mode)
{
  switch (mode)
  {
    case HistoryMode::Deterministic:
      return "deterministic";
    case HistoryMode::Single:
      return "single";
    case HistoryMode::Poisson:
      return "poisson";
  }
  return "unknown";
}

HistoryConfig ReadHistoryConfig(std::istream& in, const std::filesystem::path& baseDirectory)
{
  // Phase 1: collect `key = value` lines, rejecting syntax errors, unknown keys
  // and repeated keys.
  std::map<std::string, Entry> collected;
  std::string text;
  std::size_t lineNumber = 0;
  while (std::getline(in, text))
  {
    ++lineNumber;
    const auto line = Trim(text);
    if (line.empty() || line[0] == '#') continue;
    const std::string where = "line " + std::to_string(lineNumber) + ": ";
    const auto equals = line.find('=');
    Require(equals != std::string::npos, where + "expected 'key = value', found '" + line + "'");
    const auto key = Trim(line.substr(0, equals));
    const auto value = Trim(line.substr(equals + 1));
    Require(!key.empty(), where + "missing key before '='");
    Require(IsKnownKey(key), where + "unknown key '" + key + "'");
    Require(!value.empty(), where + "empty value for '" + key + "'");
    const auto [it, inserted] = collected.emplace(key, Entry{value, lineNumber});
    Require(inserted, where + "key '" + key + "' repeated (first on line " + std::to_string(it->second.line) + ")");
  }
  Require(!in.bad(), "read error");

  // Phase 2: the mode decides which keys are allowed and required.
  const Entries entries(std::move(collected));
  HistoryConfig config;
  config.mode = ParseMode(entries);
  for (const auto* key : {"mode", "libraryDir", "endpointNs", "chemistrySeed", "worldHalfSideUm", "halfBoxUm",
                          "gridMinAgeNs", "gridPointsPerDecade", "newOldContactLimit", "scheduleFile", "singleSource", "rateHz",
                          "exposureS", "sourceSeed", "regionCenterUm", "regionHalfSizeUm"})
    if (entries.Has(key))
      Require(IsKeyOfMode(key, config.mode),
              entries.Where(key) + "not used by mode " + HistoryModeName(config.mode));

  config.libraryDir = entries.Path("libraryDir", baseDirectory);
  config.endpointNs = entries.Number("endpointNs");
  Require(config.endpointNs > 0, entries.Where("endpointNs") + "must be positive");
  config.chemistrySeed = entries.Unsigned("chemistrySeed", 1, kMaxHistorySeed);

  if (entries.Has("worldHalfSideUm"))
  {
    const double worldUm = entries.Number("worldHalfSideUm");
    Require(worldUm > 0, entries.Where("worldHalfSideUm") + "must be positive");
    config.worldHalfSideNm = worldUm * 1000.0;
  }
  if (entries.Has("halfBoxUm"))
  {
    const double boxUm = entries.Number("halfBoxUm");
    Require(boxUm >= 0, entries.Where("halfBoxUm") + "must be zero (no box) or positive");
    config.halfBoxNm = boxUm * 1000.0;
    Require(config.halfBoxNm < config.worldHalfSideNm,
            entries.Where("halfBoxUm") + "the reflecting box must be smaller than the world (worldHalfSideUm)");
  }
  if (entries.Has("gridMinAgeNs"))
  {
    config.gridMinAgeNs = entries.Number("gridMinAgeNs");
    Require(config.gridMinAgeNs >= 0.01, entries.Where("gridMinAgeNs") + "must be at least 0.01 ns");
  }
  Require(config.gridMinAgeNs < config.endpointNs, "gridMinAgeNs must be smaller than endpointNs");
  if (entries.Has("gridPointsPerDecade"))
    config.gridPointsPerDecade = static_cast<int>(entries.Unsigned("gridPointsPerDecade", 1, 100));

  if (entries.Has("newOldContactLimit"))
    config.newOldContactLimit = entries.Unsigned("newOldContactLimit", 0, kMaxContactLimit);

  if (config.mode == HistoryMode::Deterministic || config.mode == HistoryMode::Single)
    config.scheduleFile = entries.Path("scheduleFile", baseDirectory);
  if (config.mode == HistoryMode::Single)
    config.singleSource = static_cast<std::size_t>(entries.Unsigned("singleSource", 0, 99999));

  if (config.mode == HistoryMode::Poisson)
  {
    auto& poisson = config.poisson;
    poisson.rateHz = entries.Number("rateHz");
    Require(poisson.rateHz > 0, entries.Where("rateHz") + "must be positive");
    poisson.exposureS = entries.Number("exposureS");
    Require(poisson.exposureS > 0, entries.Where("exposureS") + "must be positive");
    Require(poisson.exposureS * 1e9 < config.endpointNs,
            entries.Where("exposureS") + "the endpoint must be later than the exposure window");
    poisson.sourceSeed = entries.Unsigned("sourceSeed", 1, kMaxHistorySeed);
    poisson.regionCenterNm = {0, 0, 0};
    if (entries.Has("regionCenterUm"))
    {
      const auto centre = entries.Numbers("regionCenterUm");
      Require(centre.size() == 3, entries.Where("regionCenterUm") + "expected three numbers (x y z)");
      for (int axis = 0; axis < 3; ++axis) poisson.regionCenterNm[axis] = centre[axis] * 1000.0;
    }
    poisson.regionHalfSizeNm = {50000, 50000, 50000};
    if (entries.Has("regionHalfSizeUm"))
    {
      const auto half = entries.Numbers("regionHalfSizeUm");
      Require(half.size() == 1 || half.size() == 3,
              entries.Where("regionHalfSizeUm") + "expected one number or three (x y z)");
      for (int axis = 0; axis < 3; ++axis)
      {
        const double value = half.size() == 1 ? half[0] : half[axis];
        Require(value >= 0, entries.Where("regionHalfSizeUm") + "must not be negative");
        poisson.regionHalfSizeNm[axis] = value * 1000.0;
      }
    }
  }
  return config;
}

HistoryConfig ReadHistoryConfigFile(const std::filesystem::path& file)
{
  std::ifstream in(file);
  Require(static_cast<bool>(in), "cannot open " + file.string());
  auto base = file.parent_path();
  if (base.empty()) base = ".";
  try
  {
    return ReadHistoryConfig(in, base);
  }
  catch (const std::runtime_error& error)
  {
    throw std::runtime_error(file.filename().string() + ": " + error.what());
  }
}

std::vector<double> BuildRecordGrid(const std::vector<double>& arrivalTimesNs, double endpointNs,
                                    double minAgeNs, int pointsPerDecade)
{
  Require(std::isfinite(endpointNs) && endpointNs > 0, "record grid: the endpoint must be positive and finite");
  Require(std::isfinite(minAgeNs) && minAgeNs > 0, "record grid: the minimum age must be positive and finite");
  Require(pointsPerDecade >= 1 && pointsPerDecade <= 100, "record grid: points per decade must be in [1, 100]");

  std::vector<double> arrivals = arrivalTimesNs;
  for (const double t : arrivals)
    Require(std::isfinite(t) && t >= 0 && t < endpointNs,
            "record grid: an arrival time is not in [0, endpoint)");
  std::sort(arrivals.begin(), arrivals.end());
  arrivals.erase(std::unique(arrivals.begin(), arrivals.end()), arrivals.end());

  // True when t is within the guard of an arrival time.
  const auto nearArrival = [&arrivals](double t) {
    const auto next = std::lower_bound(arrivals.begin(), arrivals.end(), t);
    if (next != arrivals.end() && *next - t < kGridGuardNs) return true;
    return next != arrivals.begin() && t - *(next - 1) < kGridGuardNs;
  };

  std::vector<double> anchors{0.0};
  anchors.insert(anchors.end(), arrivals.begin(), arrivals.end());
  anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());

  std::vector<double> grid;
  for (const double anchor : anchors)
    for (int k = 0;; ++k)
    {
      const double t = anchor + minAgeNs * std::pow(10.0, static_cast<double>(k) / pointsPerDecade);
      if (!(t < endpointNs)) break;
      if (nearArrival(t)) continue;
      grid.push_back(t);
      Require(grid.size() < kMaxGridPoints, "record grid: more than " + std::to_string(kMaxGridPoints) +
                                                " points; lower gridPointsPerDecade or the arrival count");
    }
  grid.push_back(endpointNs);
  std::sort(grid.begin(), grid.end());
  grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
  return grid;
}

}  // namespace ptirt
