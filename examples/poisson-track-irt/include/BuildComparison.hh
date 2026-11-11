// BuildComparison: stock-vs-patched comparison of species counts, clustered by
// physical packet group (Geant4-free).
//
// Question. The stock build captures its packets after the start-up contacts
// and misses the new-old contacts at later activations; the patched build does
// not. Do the two builds still give the same species counts?
//
// Design. A cluster is one packet library (the same physical packets, captured
// once per build; their chemistry-start digests are equal, which the driving
// script asserts). Inside a cluster the histories are paired by seed: the same
// chemistry seed in both builds. For each record time and species:
//
//   d_k    = mean over the seeds of cluster k of (stock - patched)
//   mean   = mean of d_k over the K clusters
//   CI     = mean +- t(0.975, K-1) * sd(d_k) / sqrt(K)        (sd with K-1)
//   margin = max(0.05 * patchedMean, 1)         patchedMean: mean over all patched histories
//
// Clusters, not histories, are the unit of replication: histories of one
// cluster share their packets and are not independent samples of the packet
// ensemble.
//
// Verdict:
//   EMPTY         patchedMean == 0 and every d_k == 0 (the species is absent in both builds)
//   EQUIVALENT    CI inside [-margin, +margin]
//   DIFFERENT     CI does not meet [-margin, +margin]
//   INCONCLUSIVE  anything else
// biasFlag is set when the CI excludes 0, whatever the verdict: a systematic
// shift smaller than the margin is reported, not hidden. The margin is fixed
// by the plan and is never changed to make a comparison pass.
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "SeparatedCheck.hh"

namespace ptirt
{

inline constexpr double kComparisonRelativeMargin = 0.05;
inline constexpr double kComparisonAbsoluteMargin = 1.0;  // molecules

inline constexpr const char* kVerdictEquivalent = "EQUIVALENT";
inline constexpr const char* kVerdictDifferent = "DIFFERENT";
inline constexpr const char* kVerdictInconclusive = "INCONCLUSIVE";
inline constexpr const char* kVerdictEmpty = "EMPTY";

// Two-sided 95 % Student t quantile (0.975) for df = 1..30. Throws
// std::invalid_argument for any other df.
double StudentT95(int df);

struct ClusterDiff
{
  double timeNs = 0;
  std::string species;
  std::vector<double> clusterMeanDiffs;  // stock - patched, one per cluster
  double patchedMean = 0;
};

struct ComparisonRow
{
  double timeNs = 0;
  std::string species;
  std::size_t clusters = 0;
  double patchedMean = 0;
  double meanDiff = 0;
  double ciLow = 0, ciHigh = 0;
  double margin = 0;
  std::string verdict;
  bool biasFlag = false;
};

// Throws std::invalid_argument unless there are 2..31 clusters and the values are finite.
ComparisonRow Compare(const ClusterDiff& diff);

// One finished history: its cluster, its seed index within the cluster, its species table.
struct BuildHistory
{
  std::size_t cluster = 0;
  std::size_t seed = 0;
  SpeciesTable table;
};

// Pairs stock and patched histories by (cluster, seed) and compares every record
// time and species. Throws std::runtime_error unless both builds hold the same
// (cluster, seed) keys, at least two clusters, and identical record times and
// species in every history. Rows are in the order of the record times and species.
std::vector<ComparisonRow> CompareBuilds(const std::vector<BuildHistory>& stock,
                                         const std::vector<BuildHistory>& patched);

struct VerdictSummary
{
  std::size_t equivalent = 0, different = 0, inconclusive = 0, empty = 0;
  std::size_t biasFlags = 0;       // rows with biasFlag
  std::size_t biasEquivalent = 0;  // biased rows that are still EQUIVALENT
  std::size_t Total() const { return equivalent + different + inconclusive + empty; }
};

VerdictSummary Summarize(const std::vector<ComparisonRow>& rows);

// Header: timeNs,species,clusters,patchedMean,meanDiff,ciLow,ciHigh,margin,verdict,biasFlag
// (17 significant digits; biasFlag 0/1).
void WriteComparisonCsv(std::ostream& out, const std::vector<ComparisonRow>& rows);

}  // namespace ptirt
