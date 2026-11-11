#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

#include "BuildComparison.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

ClusterDiff Diff(std::vector<double> diffs, double patchedMean)
{
  ClusterDiff d;
  d.timeNs = 100;
  d.species = "e_aq";
  d.clusterMeanDiffs = std::move(diffs);
  d.patchedMean = patchedMean;
  return d;
}

void TestStudentTable()
{
  CHECK(std::fabs(StudentT95(7) - 2.364624) < 1e-6);
  CHECK(std::fabs(StudentT95(1) - 12.706205) < 1e-6);
  CHECK(std::fabs(StudentT95(30) - 2.042272) < 1e-6);
  // The table decreases with df.
  for (int df = 2; df <= 30; ++df) CHECK(StudentT95(df) < StudentT95(df - 1));
  CHECK_THROWS(StudentT95(0), "df must be 1..30");
  CHECK_THROWS(StudentT95(31), "df must be 1..30");
  CHECK_THROWS(StudentT95(-1), "df must be 1..30");
}

void TestVerdicts()
{
  const auto equivalent = Compare(Diff({0, 0, 0, 0}, 100));
  CHECK(equivalent.verdict == kVerdictEquivalent);
  CHECK(!equivalent.biasFlag);
  CHECK(equivalent.margin == 5);
  CHECK(equivalent.clusters == 4);

  const auto different = Compare(Diff({10, 10, 10, 10}, 100));
  CHECK(different.verdict == kVerdictDifferent);
  CHECK(different.biasFlag);
  CHECK(different.meanDiff == 10 && different.ciLow == 10 && different.ciHigh == 10);

  const auto negative = Compare(Diff({-10, -10, -10}, 100));
  CHECK(negative.verdict == kVerdictDifferent && negative.biasFlag);

  // A wide spread: the CI contains 0 and sticks out of +-5.
  const auto wide = Compare(Diff({-20, 20, -15, 15, 0, 5, -5, 10}, 100));
  CHECK(wide.verdict == kVerdictInconclusive);
  CHECK(!wide.biasFlag);
  CHECK(wide.ciLow < -wide.margin && wide.ciHigh > wide.margin);

  const auto empty = Compare(Diff({0, 0, 0}, 0));
  CHECK(empty.verdict == kVerdictEmpty);
  CHECK(!empty.biasFlag);
  CHECK(empty.margin == 1);
}

void TestMarginAndBias()
{
  // Margin: max(5 % of the patched mean, 1 molecule).
  CHECK(Compare(Diff({0, 0}, 10)).margin == 1);
  CHECK(Compare(Diff({0, 0}, 1000)).margin == 50);
  // A small constant shift is EQUIVALENT but flagged as a bias.
  const auto biased = Compare(Diff({2, 2, 2, 2}, 100));
  CHECK(biased.verdict == kVerdictEquivalent && biased.biasFlag);
  // Shift of exactly the margin: the CI [5,5] is inside [-5,5].
  CHECK(Compare(Diff({5, 5, 5}, 100)).verdict == kVerdictEquivalent);
  // Patched mean 0 but the stock build has molecules: not EMPTY.
  const auto stockOnly = Compare(Diff({4, 4, 4}, 0));
  CHECK(stockOnly.verdict == kVerdictDifferent && stockOnly.biasFlag);
  // Confidence interval of a known sample: diffs 1,2,3,4 -> mean 2.5, sd sqrt(5/3), t(3) = 3.182446.
  const auto known = Compare(Diff({1, 2, 3, 4}, 1000));
  const double half = 3.182446 * std::sqrt(5.0 / 3.0) / 2.0;
  CHECK(std::fabs(known.ciLow - (2.5 - half)) < 1e-9);
  CHECK(std::fabs(known.ciHigh - (2.5 + half)) < 1e-9);
}

void TestRefusals()
{
  CHECK_THROWS(Compare(Diff({1}, 10)), "2..31 clusters");
  CHECK_THROWS(Compare(Diff(std::vector<double>(32, 0), 10)), "2..31 clusters");
  CHECK(Compare(Diff(std::vector<double>(31, 0), 10)).clusters == 31);
  CHECK_THROWS(Compare(Diff({0, std::nan("")}, 10)), "not finite");
  CHECK_THROWS(Compare(Diff({0, 0}, -1)), "patched mean");
}

SpeciesTable Table(const std::vector<double>& e, const std::vector<double>& h)
{
  SpeciesTable table;
  table.timesNs = {1, 10};
  table.species = {"e_aq", "H"};
  table.counts = {e[0], h[0], e[1], h[1]};
  return table;
}

BuildHistory History(std::size_t cluster, std::size_t seed, double e0, double h0, double e1, double h1)
{
  return BuildHistory{cluster, seed, Table({e0, e1}, {h0, h1})};
}

void TestCompareBuilds()
{
  // Two clusters of two seeds. Stock - patched, e_aq at t=1: cluster 0: mean(+2, +4) = 3, cluster 1: mean(-1, +1) = 0.
  std::vector<BuildHistory> stock = {History(0, 0, 52, 0, 10, 0), History(0, 1, 54, 0, 10, 0),
                                     History(1, 0, 49, 0, 10, 0), History(1, 1, 51, 0, 10, 0)};
  std::vector<BuildHistory> patched = {History(0, 0, 50, 0, 10, 0), History(0, 1, 50, 0, 10, 0),
                                       History(1, 0, 50, 0, 10, 0), History(1, 1, 50, 0, 10, 0)};
  const auto rows = CompareBuilds(stock, patched);
  CHECK(rows.size() == 4);  // 2 times x 2 species, time-major
  CHECK(rows[0].timeNs == 1 && rows[0].species == "e_aq");
  CHECK(rows[1].timeNs == 1 && rows[1].species == "H");
  CHECK(rows[2].timeNs == 10 && rows[2].species == "e_aq");
  CHECK(rows[0].clusters == 2);
  CHECK(rows[0].patchedMean == 50);
  CHECK(std::fabs(rows[0].meanDiff - 1.5) < 1e-12);
  CHECK(rows[1].verdict == kVerdictEmpty);   // H is 0 everywhere
  CHECK(rows[2].verdict == kVerdictEquivalent && !rows[2].biasFlag);  // e_aq at 10: identical
  const auto summary = Summarize(rows);
  CHECK(summary.Total() == 4);
  CHECK(summary.empty == 2 && summary.equivalent + summary.different + summary.inconclusive == 2);

  std::ostringstream csv;
  WriteComparisonCsv(csv, rows);
  CHECK(csv.str().rfind("timeNs,species,clusters,patchedMean,meanDiff,ciLow,ciHigh,margin,verdict,biasFlag\n", 0) == 0);
  CHECK(csv.str().find(",EMPTY,0\n") != std::string::npos);

  // Mismatched key sets, a single cluster, a different grid, a repeated key.
  auto missing = patched;
  missing.pop_back();
  CHECK_THROWS(CompareBuilds(stock, missing), "in the stock set but not in the patched set");
  CHECK_THROWS(CompareBuilds(missing, stock), "in the patched set but not in the stock set");
  CHECK_THROWS(CompareBuilds({History(0, 0, 1, 1, 1, 1)}, {History(0, 0, 1, 1, 1, 1)}), "at least 2 clusters");
  auto grid = patched;
  grid[1].table.timesNs[1] = 11;
  CHECK_THROWS(CompareBuilds(stock, grid), "record times or species differ");
  auto twice = stock;
  twice.push_back(stock[0]);
  CHECK_THROWS(CompareBuilds(twice, patched), "appears twice");
  CHECK_THROWS(CompareBuilds({}, {}), "no histories");
}

}  // namespace

int main()
{
  TestStudentTable();
  TestVerdicts();
  TestMarginAndBias();
  TestRefusals();
  TestCompareBuilds();
  std::puts("BuildComparisonTest OK");
  return 0;
}
