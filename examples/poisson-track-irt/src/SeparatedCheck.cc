#include "SeparatedCheck.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace ptirt
{
namespace
{

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error("SeparatedCheck: " + message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

std::string Real(double x)
{
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "%.17g", x);
  return buffer;
}

std::string Short(double x)
{
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "%.6g", x);
  return buffer;
}

std::string CsvText(const std::string& text)
{
  if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
  std::string quoted = "\"";
  for (char c : text)
  {
    if (c == '"') quoted += '"';
    quoted += c;
  }
  return quoted + '"';
}

// Splits one CSV line; a field may be double-quoted (with "" for a quote).
std::vector<std::string> SplitCsv(const std::string& line)
{
  std::vector<std::string> fields(1);
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i)
  {
    const char c = line[i];
    if (quoted)
    {
      if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
      {
        fields.back() += '"';
        ++i;
      }
      else if (c == '"')
        quoted = false;
      else
        fields.back() += c;
    }
    else if (c == '"')
      quoted = true;
    else if (c == ',')
      fields.emplace_back();
    else if (c != '\r')
      fields.back() += c;
  }
  return fields;
}

double ParseNumber(const std::string& text, const std::string& where)
{
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  Require(!text.empty() && end == text.c_str() + text.size() && std::isfinite(value),
          where + "malformed number '" + text + "'");
  return value;
}

struct Moments
{
  double mean = 0;
  double standardError = 0;
};

// Mean and standard error of the mean (sample standard deviation, N - 1).
Moments MeanAndError(const std::vector<double>& values)
{
  const double n = static_cast<double>(values.size());
  double sum = 0;
  for (double v : values) sum += v;
  Moments moments;
  moments.mean = sum / n;
  double squares = 0;
  for (double v : values) squares += (v - moments.mean) * (v - moments.mean);
  moments.standardError = std::sqrt(squares / (n - 1.0) / n);
  return moments;
}

void SameGrid(const SpeciesTable& reference, const SpeciesTable& table, const std::string& label)
{
  Require(table.timesNs == reference.timesNs, label + ": the record times differ from the first table's");
  Require(table.species == reference.species, label + ": the species differ from the first table's");
}

void Tally(GroupSummary& group, const CellResult& cell, const std::string& where)
{
  ++group.cells;
  if (!cell.pass) ++group.failed;
  switch (cell.basis)
  {
    case Basis::Statistical: ++group.byStatistics; break;
    case Basis::Relative: ++group.byRelative; break;
    case Basis::Floor: ++group.byFloor; break;
    case Basis::Failed: break;
  }
  if (group.worstWhere.empty() || cell.ratio > group.worstRatio)
  {
    group.worstRatio = cell.ratio;
    group.worstWhere = where;
  }
}

JsonValue GroupJson(const GroupSummary& g, bool withTime)
{
  auto item = JsonValue::Object();
  if (withTime)
  {
    item.Set("timeNs", JsonValue::Real(g.timeNs));
    item.Set("ageNs", JsonValue::Real(g.ageNs));
  }
  else
    item.Set("species", JsonValue::String(g.label));
  item.Set("cells", JsonValue::Integer(static_cast<long long>(g.cells)));
  item.Set("failed", JsonValue::Integer(static_cast<long long>(g.failed)));
  item.Set("worstRatio", JsonValue::Real(g.worstRatio));
  item.Set(withTime ? "worstSpecies" : "worstTime", JsonValue::String(g.worstWhere));
  return item;
}

}  // namespace

const char* BasisName(Basis basis)
{
  switch (basis)
  {
    case Basis::Statistical: return "statistical";
    case Basis::Relative: return "relative";
    case Basis::Floor: return "floor";
    case Basis::Failed: return "failed";
  }
  return "?";
}

SpeciesTable ReadSpeciesTable(std::istream& in, const std::string& label)
{
  SpeciesTable table;
  std::string line;
  std::size_t lineNumber = 0;
  const auto where = [&](std::size_t n) { return label + " line " + std::to_string(n) + ": "; };

  Require(static_cast<bool>(std::getline(in, line)), label + ": empty file");
  ++lineNumber;
  const auto header = SplitCsv(line);
  Require(header.size() >= 3 && header[0] == "timeNs" && header[1] == "species" && header[2] == "count",
          label + ": the header must start with timeNs,species,count");

  std::vector<std::string> block;  // species seen at the current time
  std::vector<double> blockCounts;
  const auto closeBlock = [&](std::size_t n) {
    if (table.timesNs.empty()) return;
    if (table.timesNs.size() == 1)
      table.species = block;
    else
      Require(block == table.species, where(n) + "the species listed at time " + Real(table.timesNs.back()) +
                                          " differ from those at the first time");
    table.counts.insert(table.counts.end(), blockCounts.begin(), blockCounts.end());
    block.clear();
    blockCounts.clear();
  };

  while (std::getline(in, line))
  {
    ++lineNumber;
    if (line.empty() || line == "\r") continue;
    const auto fields = SplitCsv(line);
    Require(fields.size() >= 3, where(lineNumber) + "expected at least 3 fields");
    const double time = ParseNumber(fields[0], where(lineNumber));
    const double count = ParseNumber(fields[2], where(lineNumber));
    Require(count >= 0, where(lineNumber) + "negative count");
    if (table.timesNs.empty() || time != table.timesNs.back())
    {
      Require(table.timesNs.empty() || time > table.timesNs.back(),
              where(lineNumber) + "record times must be strictly increasing");
      closeBlock(lineNumber);
      table.timesNs.push_back(time);
    }
    Require(std::find(block.begin(), block.end(), fields[1]) == block.end(),
            where(lineNumber) + "species '" + fields[1] + "' listed twice at one time");
    block.push_back(fields[1]);
    blockCounts.push_back(count);
  }
  closeBlock(lineNumber + 1);
  Require(!table.timesNs.empty(), label + ": no data rows");
  return table;
}

CheckResult CompareSeparated(const std::vector<SpeciesTable>& combined,
                             const std::vector<SingleEnsemble>& singles, const CheckOptions& options)
{
  Require(options.sigmas > 0 && options.relativeTolerance >= 0 && options.absoluteFloor >= 0,
          "invalid tolerance options");
  Require(!singles.empty(), "no source: at least one single-source ensemble is needed");
  Require(combined.size() >= 2, "the combined ensemble needs at least 2 seeds");
  for (std::size_t k = 0; k < singles.size(); ++k)
    Require(singles[k].tables.size() == singles.front().tables.size(),
            "source " + std::to_string(k) + " has " + std::to_string(singles[k].tables.size()) +
                " single histories, source 0 has " + std::to_string(singles.front().tables.size()));
  const std::size_t singleSeeds = singles.front().tables.size();
  Require(singleSeeds >= 2, "each single-source ensemble needs at least 2 seeds");
  for (std::size_t k = 0; k < singles.size(); ++k)
  {
    Require(std::isfinite(singles[k].activationNs) && singles[k].activationNs >= 0,
            "source " + std::to_string(k) + ": the activation time must be finite and >= 0");
    for (std::size_t j = 0; j < k; ++j)
      Require(singles[j].activationNs != singles[k].activationNs,
              "sources " + std::to_string(j) + " and " + std::to_string(k) + " share an activation time");
  }

  const SpeciesTable& reference = combined.front();
  for (std::size_t i = 0; i < combined.size(); ++i)
    SameGrid(reference, combined[i], "combined history " + std::to_string(i));

  CheckResult result;
  result.options = options;
  result.combinedSeeds = combined.size();
  result.singleSeeds = singleSeeds;
  result.sources = singles.size();
  for (std::size_t k = 0; k < singles.size(); ++k)
  {
    result.activationsNs.push_back(singles[k].activationNs);
    for (std::size_t i = 0; i < singleSeeds; ++i)
    {
      const std::string label = "single history of source " + std::to_string(k) + ", seed index " + std::to_string(i);
      const auto& table = singles[k].tables[i];
      SameGrid(reference, table, label);
      // Nothing can exist before the source activates.
      for (std::size_t t = 0; t < table.timesNs.size() && table.timesNs[t] < singles[k].activationNs; ++t)
        for (std::size_t s = 0; s < table.species.size(); ++s)
          Require(table.At(t, s) == 0, label + ": nonzero count of " + table.species[s] + " at " +
                                           Real(table.timesNs[t]) + " ns, before the activation at " +
                                           Real(singles[k].activationNs) + " ns");
    }
  }

  const std::size_t nTimes = reference.timesNs.size();
  const std::size_t nSpecies = reference.species.size();
  std::vector<double> combinedValues(combined.size());
  std::vector<double> independentValues(singleSeeds);
  result.minAgeNs = HUGE_VAL;
  result.maxAgeNs = 0;
  result.perTime.resize(nTimes);
  result.perSpecies.resize(nSpecies);

  for (std::size_t t = 0; t < nTimes; ++t)
  {
    const double time = reference.timesNs[t];
    // Age since the latest activation at or before this time (NaN before the first).
    double latest = -1;
    for (const double activation : result.activationsNs)
      if (activation <= time && activation > latest) latest = activation;
    const double age = latest < 0 ? std::nan("") : time - latest;
    if (std::isfinite(age))
    {
      result.minAgeNs = std::min(result.minAgeNs, age);
      result.maxAgeNs = std::max(result.maxAgeNs, age);
    }
    result.perTime[t].label = "t=" + Short(time) + " ns";
    result.perTime[t].timeNs = time;
    result.perTime[t].ageNs = age;

    for (std::size_t s = 0; s < nSpecies; ++s)
    {
      for (std::size_t i = 0; i < combined.size(); ++i) combinedValues[i] = combined[i].At(t, s);
      for (std::size_t i = 0; i < singleSeeds; ++i)
      {
        double sum = 0;
        for (const auto& source : singles) sum += source.tables[i].At(t, s);
        independentValues[i] = sum;
      }
      const Moments c = MeanAndError(combinedValues);
      const Moments e = MeanAndError(independentValues);

      CellResult cell;
      cell.timeIndex = t;
      cell.speciesIndex = s;
      cell.timeNs = time;
      cell.ageNs = age;
      cell.species = reference.species[s];
      cell.combinedMean = c.mean;
      cell.combinedSe = c.standardError;
      cell.independentMean = e.mean;
      cell.independentSe = e.standardError;
      cell.difference = c.mean - e.mean;
      cell.se = std::sqrt(c.standardError * c.standardError + e.standardError * e.standardError);
      const double statistical = options.sigmas * cell.se;
      const double relative = options.relativeTolerance * e.mean;
      cell.tolerance = statistical + relative + options.absoluteFloor;
      const double distance = std::abs(cell.difference);
      cell.ratio = distance == 0 ? 0 : distance / cell.tolerance;
      if (distance <= statistical)
        cell.basis = Basis::Statistical;
      else if (distance <= statistical + relative)
        cell.basis = Basis::Relative;
      else if (distance <= cell.tolerance)
        cell.basis = Basis::Floor;
      else
        cell.basis = Basis::Failed;
      cell.pass = cell.basis != Basis::Failed;

      if (!cell.pass) ++result.failed;
      if (cell.basis == Basis::Statistical) ++result.byStatistics;
      if (cell.basis == Basis::Relative) ++result.byRelative;
      if (cell.basis == Basis::Floor) ++result.byFloor;
      Tally(result.perTime[t], cell, cell.species);
      auto& group = result.perSpecies[s];
      group.label = cell.species;
      Tally(group, cell, "t=" + Short(time) + " ns");
      result.cells.push_back(cell);
    }
  }
  if (!std::isfinite(result.minAgeNs)) result.minAgeNs = 0;
  return result;
}

void WriteCellsCsv(std::ostream& out, const CheckResult& result)
{
  out << "timeNs,ageNs,species,combinedMean,combinedSe,independentMean,independentSe,difference,se,"
         "tolerance,ratio,basis,pass\n";
  for (const auto& c : result.cells)
    out << Real(c.timeNs) << ',' << (std::isfinite(c.ageNs) ? Real(c.ageNs) : std::string()) << ','
        << CsvText(c.species) << ',' << Real(c.combinedMean) << ',' << Real(c.combinedSe) << ','
        << Real(c.independentMean) << ',' << Real(c.independentSe) << ',' << Real(c.difference) << ','
        << Real(c.se) << ',' << Real(c.tolerance) << ',' << Real(c.ratio) << ',' << BasisName(c.basis) << ','
        << (c.pass ? "true" : "false") << '\n';
  if (!out) Fail("cells stream failed");
}

JsonValue MakeSummaryJson(const CheckResult& result)
{
  const auto count = [](std::size_t n) { return JsonValue::Integer(static_cast<long long>(n)); };
  auto root = JsonValue::Object();
  root.Set("format", JsonValue::String("ptirt-separated-check"));
  root.Set("formatVersion", JsonValue::Integer(1));
  root.Set("pass", JsonValue::Bool(result.Pass()));
  auto options = JsonValue::Object();
  options.Set("sigmas", JsonValue::Real(result.options.sigmas));
  options.Set("relativeTolerance", JsonValue::Real(result.options.relativeTolerance));
  options.Set("absoluteFloor_molecules", JsonValue::Real(result.options.absoluteFloor));
  options.Set("rule", JsonValue::String("|combinedMean - independentMean| <= sigmas * sqrt(seC^2 + seE^2) + "
                                        "relativeTolerance * independentMean + absoluteFloor"));
  root.Set("tolerance", options);
  auto inputs = JsonValue::Object();
  inputs.Set("combinedSeeds", count(result.combinedSeeds));
  inputs.Set("singleSeedsPerSource", count(result.singleSeeds));
  inputs.Set("sources", count(result.sources));
  auto activations = JsonValue::Array();
  for (const double a : result.activationsNs) activations.Append(JsonValue::Real(a));
  inputs.Set("activations_ns", activations);
  inputs.Set("recordTimes", count(result.perTime.size()));
  inputs.Set("species", count(result.perSpecies.size()));
  inputs.Set("minAge_ns", JsonValue::Real(result.minAgeNs));
  inputs.Set("maxAge_ns", JsonValue::Real(result.maxAgeNs));
  root.Set("inputs", inputs);
  auto totals = JsonValue::Object();
  totals.Set("cells", count(result.cells.size()));
  totals.Set("failed", count(result.failed));
  totals.Set("passedByStatistics", count(result.byStatistics));
  totals.Set("passedByRelativeTerm", count(result.byRelative));
  totals.Set("passedByFloor", count(result.byFloor));
  root.Set("totals", totals);
  auto species = JsonValue::Array();
  for (const auto& g : result.perSpecies) species.Append(GroupJson(g, false));
  root.Set("perSpecies", species);
  auto times = JsonValue::Array();
  for (const auto& g : result.perTime) times.Append(GroupJson(g, true));
  root.Set("perTime", times);
  return root;
}

void WriteTextSummary(std::ostream& out, const CheckResult& result, std::size_t worstCellCount)
{
  Require(!result.cells.empty(), "no cells to summarize");
  char line[512];
  out << "Separated-sources check: combined history vs the sum of independent single-source histories\n";
  out << "combined seeds " << result.combinedSeeds << ", single-source seeds " << result.singleSeeds
      << " per source, sources " << result.sources << ", activations (ns):";
  for (const double a : result.activationsNs) out << ' ' << Short(a);
  out << "\nrecord times " << result.perTime.size() << ", species " << result.perSpecies.size() << ", cells "
      << result.cells.size() << ", ages since the latest activation " << Short(result.minAgeNs) << " ns to "
      << Short(result.maxAgeNs) << " ns\n";
  out << "tolerance: |C - E| <= " << Short(result.options.sigmas) << " * sqrt(SE_C^2 + SE_E^2) + "
      << Short(result.options.relativeTolerance) << " * E + " << Short(result.options.absoluteFloor)
      << " molecule(s)   (C combined mean, E sum of the single-source means)\n";
  out << "cells: failed " << result.failed << ", passed within the statistical term " << result.byStatistics
      << ", needing the relative term " << result.byRelative << ", needing the absolute floor " << result.byFloor
      << "\n\n";

  // Per species: failures, worst ratio, and the final record time.
  const std::size_t nSpecies = result.perSpecies.size();
  const std::size_t lastTime = result.perTime.size() - 1;
  out << "Per species (ratio = |C - E| / tolerance, 1 = at the limit); final record time "
      << Short(result.perTime[lastTime].timeNs) << " ns\n";
  std::snprintf(line, sizeof line, "%-10s %6s %6s %9s %-18s | %11s %11s %9s %7s\n", "species", "cells", "failed",
                "maxRatio", "at", "C(final)", "E(final)", "SE", "ratio");
  out << line;
  for (std::size_t s = 0; s < nSpecies; ++s)
  {
    const auto& g = result.perSpecies[s];
    const auto& f = result.cells[lastTime * nSpecies + s];
    std::snprintf(line, sizeof line, "%-10s %6zu %6zu %9.3f %-18s | %11.5g %11.5g %9.3g %7.3f\n", g.label.c_str(),
                  g.cells, g.failed, g.worstRatio, g.worstWhere.c_str(), f.combinedMean, f.independentMean, f.se,
                  f.ratio);
    out << line;
  }

  out << "\nPer record time (all species)\n";
  std::snprintf(line, sizeof line, "%14s %14s %6s %9s %s\n", "timeNs", "ageNs", "failed", "maxRatio", "worst species");
  out << line;
  for (const auto& g : result.perTime)
  {
    std::snprintf(line, sizeof line, "%14.8g %14.8g %6zu %9.3f %s\n", g.timeNs, g.ageNs, g.failed, g.worstRatio,
                  g.worstWhere.c_str());
    out << line;
  }

  std::vector<const CellResult*> order;
  for (const auto& c : result.cells) order.push_back(&c);
  std::stable_sort(order.begin(), order.end(),
                   [](const CellResult* a, const CellResult* b) { return a->ratio > b->ratio; });
  out << "\nWorst cells\n";
  std::snprintf(line, sizeof line, "%14s %12s %-10s %11s %11s %9s %9s %9s %s\n", "timeNs", "ageNs", "species", "C",
                "E", "diff", "tol", "ratio", "basis");
  out << line;
  for (std::size_t i = 0; i < std::min(worstCellCount, order.size()); ++i)
  {
    const auto& c = *order[i];
    std::snprintf(line, sizeof line, "%14.8g %12.6g %-10s %11.5g %11.5g %9.3g %9.3g %9.3f %s\n", c.timeNs, c.ageNs,
                  c.species.c_str(), c.combinedMean, c.independentMean, c.difference, c.tolerance, c.ratio,
                  BasisName(c.basis));
    out << line;
  }

  out << '\n'
      << (result.Pass() ? "SEPARATED_CHECK_PASS" : "SEPARATED_CHECK_FAIL") << " cells=" << result.cells.size()
      << " failed=" << result.failed << " timeCount=" << result.perTime.size() << " species=" << nSpecies
      << " combinedSeeds=" << result.combinedSeeds << " singleSeedsPerSource=" << result.singleSeeds
      << " sources=" << result.sources << '\n';
  if (!out) Fail("summary stream failed");
}

}  // namespace ptirt
