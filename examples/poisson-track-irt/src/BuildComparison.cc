#include "BuildComparison.hh"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ptirt
{

namespace
{

// t(0.975, df) for df = 1..30 (standard two-sided 95 % table values).
constexpr double kStudentT95[30] = {
    12.706205, 4.302653, 3.182446, 2.776445, 2.570582, 2.446912, 2.364624, 2.306004, 2.262157, 2.228139,
    2.200985,  2.178813, 2.160369, 2.144787, 2.131450, 2.119905, 2.109816, 2.100922, 2.093024, 2.085963,
    2.079614,  2.073873, 2.068658, 2.063899, 2.059539, 2.055529, 2.051831, 2.048407, 2.045230, 2.042272};

std::string Real(double value)
{
  std::ostringstream text;
  text << std::setprecision(17) << value;
  return text.str();
}

std::string KeyText(const std::pair<std::size_t, std::size_t>& key)
{
  return "cluster " + std::to_string(key.first) + " seed " + std::to_string(key.second);
}

}  // namespace

double StudentT95(int df)
{
  if (df < 1 || df > 30) throw std::invalid_argument("StudentT95: df must be 1..30, got " + std::to_string(df));
  return kStudentT95[df - 1];
}

ComparisonRow Compare(const ClusterDiff& diff)
{
  const std::size_t n = diff.clusterMeanDiffs.size();
  if (n < 2 || n > 31)
    throw std::invalid_argument("Compare: 2..31 clusters are needed, got " + std::to_string(n));
  if (!std::isfinite(diff.patchedMean) || diff.patchedMean < 0)
    throw std::invalid_argument("Compare: the patched mean must be finite and >= 0");
  double sum = 0;
  bool allZero = true;
  for (const double d : diff.clusterMeanDiffs)
  {
    if (!std::isfinite(d)) throw std::invalid_argument("Compare: a cluster difference is not finite");
    sum += d;
    if (d != 0) allZero = false;
  }
  const double mean = sum / static_cast<double>(n);
  double squares = 0;
  for (const double d : diff.clusterMeanDiffs) squares += (d - mean) * (d - mean);
  const double sd = std::sqrt(squares / static_cast<double>(n - 1));
  const double half = StudentT95(static_cast<int>(n) - 1) * sd / std::sqrt(static_cast<double>(n));

  ComparisonRow row;
  row.timeNs = diff.timeNs;
  row.species = diff.species;
  row.clusters = n;
  row.patchedMean = diff.patchedMean;
  row.meanDiff = mean;
  row.ciLow = mean - half;
  row.ciHigh = mean + half;
  row.margin = std::max(kComparisonRelativeMargin * diff.patchedMean, kComparisonAbsoluteMargin);
  row.biasFlag = row.ciLow > 0 || row.ciHigh < 0;
  if (diff.patchedMean == 0 && allZero)
    row.verdict = kVerdictEmpty;
  else if (row.ciLow >= -row.margin && row.ciHigh <= row.margin)
    row.verdict = kVerdictEquivalent;
  else if (row.ciLow > row.margin || row.ciHigh < -row.margin)
    row.verdict = kVerdictDifferent;
  else
    row.verdict = kVerdictInconclusive;
  return row;
}

std::vector<ComparisonRow> CompareBuilds(const std::vector<BuildHistory>& stock,
                                         const std::vector<BuildHistory>& patched)
{
  using Key = std::pair<std::size_t, std::size_t>;
  const auto index = [](const std::vector<BuildHistory>& histories, const char* build) {
    std::map<Key, const BuildHistory*> map;
    for (const auto& history : histories)
    {
      const Key key{history.cluster, history.seed};
      if (!map.emplace(key, &history).second)
        throw std::runtime_error(std::string(build) + ": " + KeyText(key) + " appears twice");
    }
    return map;
  };
  const auto stockMap = index(stock, "stock");
  const auto patchedMap = index(patched, "patched");
  if (stockMap.empty()) throw std::runtime_error("no histories to compare");
  for (const auto& entry : stockMap)
    if (!patchedMap.count(entry.first))
      throw std::runtime_error(KeyText(entry.first) + " is in the stock set but not in the patched set");
  for (const auto& entry : patchedMap)
    if (!stockMap.count(entry.first))
      throw std::runtime_error(KeyText(entry.first) + " is in the patched set but not in the stock set");

  const SpeciesTable& reference = stockMap.begin()->second->table;
  const auto sameGrid = [&](const SpeciesTable& table) {
    return table.timesNs == reference.timesNs && table.species == reference.species;
  };
  std::map<std::size_t, std::vector<const BuildHistory*>> stockByCluster, patchedByCluster;
  for (const auto& entry : stockMap)
  {
    const BuildHistory& s = *entry.second;
    const BuildHistory& p = *patchedMap.at(entry.first);
    if (!sameGrid(s.table) || !sameGrid(p.table))
      throw std::runtime_error(KeyText(entry.first) + ": the record times or species differ from the first history");
    stockByCluster[entry.first.first].push_back(&s);
    patchedByCluster[entry.first.first].push_back(&p);
  }
  const std::size_t clusterCount = stockByCluster.size();
  if (clusterCount < 2)
    throw std::runtime_error("at least 2 clusters are needed, found " + std::to_string(clusterCount));
  if (clusterCount > 31)
    throw std::runtime_error("at most 31 clusters are supported (Student t table to df 30), found " +
                             std::to_string(clusterCount));

  std::vector<ComparisonRow> rows;
  rows.reserve(reference.timesNs.size() * reference.species.size());
  for (std::size_t t = 0; t < reference.timesNs.size(); ++t)
    for (std::size_t sp = 0; sp < reference.species.size(); ++sp)
    {
      ClusterDiff diff;
      diff.timeNs = reference.timesNs[t];
      diff.species = reference.species[sp];
      double patchedSum = 0;
      std::size_t patchedCount = 0;
      for (const auto& [cluster, stockHistories] : stockByCluster)
      {
        const auto& patchedHistories = patchedByCluster.at(cluster);
        double clusterSum = 0;
        for (std::size_t i = 0; i < stockHistories.size(); ++i)
        {
          const double p = patchedHistories[i]->table.At(t, sp);
          clusterSum += stockHistories[i]->table.At(t, sp) - p;
          patchedSum += p;
          ++patchedCount;
        }
        diff.clusterMeanDiffs.push_back(clusterSum / static_cast<double>(stockHistories.size()));
      }
      diff.patchedMean = patchedSum / static_cast<double>(patchedCount);
      rows.push_back(Compare(diff));
    }
  return rows;
}

VerdictSummary Summarize(const std::vector<ComparisonRow>& rows)
{
  VerdictSummary summary;
  for (const auto& row : rows)
  {
    if (row.verdict == kVerdictEquivalent)
      ++summary.equivalent;
    else if (row.verdict == kVerdictDifferent)
      ++summary.different;
    else if (row.verdict == kVerdictInconclusive)
      ++summary.inconclusive;
    else
      ++summary.empty;
    if (row.biasFlag)
    {
      ++summary.biasFlags;
      if (row.verdict == kVerdictEquivalent) ++summary.biasEquivalent;
    }
  }
  return summary;
}

void WriteComparisonCsv(std::ostream& out, const std::vector<ComparisonRow>& rows)
{
  out << "timeNs,species,clusters,patchedMean,meanDiff,ciLow,ciHigh,margin,verdict,biasFlag\n";
  for (const auto& row : rows)
  {
    std::string species = row.species;
    if (species.find_first_of(",\"\r\n") != std::string::npos)
    {
      std::string quoted = "\"";
      for (const char c : species) quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
      species = quoted + "\"";
    }
    out << Real(row.timeNs) << ',' << species << ',' << row.clusters << ',' << Real(row.patchedMean) << ','
        << Real(row.meanDiff) << ',' << Real(row.ciLow) << ',' << Real(row.ciHigh) << ',' << Real(row.margin) << ','
        << row.verdict << ',' << (row.biasFlag ? 1 : 0) << '\n';
  }
}

}  // namespace ptirt
