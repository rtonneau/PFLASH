#include "SeparatedCheck.hh"

#include <cmath>
#include <functional>
#include <sstream>
#include <utility>

#include "TestCheck.hh"

using namespace ptirt;

namespace
{

const std::vector<double> kTimes{1, 10, 99, 101, 110, 200};
const std::vector<std::string> kSpecies{"A", "B"};
const std::size_t kSeeds = 16;

using Count = std::function<double(double time, std::size_t species, std::size_t seed)>;

std::vector<SpeciesTable> MakeEnsemble(const Count& count)
{
  std::vector<SpeciesTable> tables;
  for (std::size_t seed = 0; seed < kSeeds; ++seed)
  {
    SpeciesTable table;
    table.timesNs = kTimes;
    table.species = kSpecies;
    for (const double time : kTimes)
      for (std::size_t s = 0; s < kSpecies.size(); ++s) table.counts.push_back(count(time, s, seed));
    tables.push_back(table);
  }
  return tables;
}

// Zero-mean pattern over the 16 seeds: +-1, +-2, +-3, +-4 (each twice).
double Noise(std::size_t seed)
{
  const double magnitude = 1.0 + static_cast<double>((seed / 2) % 4);
  return seed % 2 == 0 ? magnitude : -magnitude;
}

// Source k alone: species A is 100 * (k + 1), B is 1 + k, both from the
// activation on; noise on A only.
double SingleCount(std::size_t source, double activation, double time, std::size_t species, std::size_t seed)
{
  if (time < activation) return 0;
  return species == 0 ? 100.0 * (static_cast<double>(source) + 1.0) + Noise(seed + source) : 1.0 + source;
}

std::vector<SingleEnsemble> MakeSingles()
{
  std::vector<SingleEnsemble> singles(2);
  singles[0].activationNs = 0;
  singles[1].activationNs = 100;
  for (std::size_t k = 0; k < 2; ++k)
    singles[k].tables = MakeEnsemble([&](double time, std::size_t species, std::size_t seed) {
      return SingleCount(k, singles[k].activationNs, time, species, seed);
    });
  return singles;
}

// The combined history: the sum of the two sources, with its own noise.
Count CombinedCount(double scaleA, double extraB)
{
  return [=](double time, std::size_t species, std::size_t seed) {
    double sum = 0;
    for (std::size_t k = 0; k < 2; ++k)
      sum += species == 0 ? (time < (k ? 100.0 : 0.0) ? 0 : 100.0 * (static_cast<double>(k) + 1.0))
                          : (time < (k ? 100.0 : 0.0) ? 0 : 1.0 + static_cast<double>(k));
    if (species == 0) return sum * scaleA + Noise(seed + 3);
    return sum + extraB;
  };
}

void TestAgreeingHistoriesPass()
{
  const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.0, 0.0)), MakeSingles(), CheckOptions{});
  CHECK(result.Pass());
  CHECK(result.failed == 0);
  CHECK(result.cells.size() == kTimes.size() * kSpecies.size());
  CHECK(result.perTime.size() == kTimes.size());
  CHECK(result.perSpecies.size() == 2);
  CHECK(result.combinedSeeds == kSeeds && result.singleSeeds == kSeeds && result.sources == 2);
  // Source 1 is not active before 100 ns: at t = 99 only source 0 counts (A = 100).
  const auto& cell = result.cells[2 * 2 + 0];
  CHECK(cell.timeNs == 99 && cell.species == "A");
  CHECK(std::abs(cell.independentMean - 100.0) < 1e-12);
  CHECK(std::abs(cell.combinedMean - 100.0) < 1e-12);
  // After 101 ns both count (A = 100 + 200).
  const auto& later = result.cells[3 * 2 + 0];
  CHECK(std::abs(later.independentMean - 300.0) < 1e-12);
}

void TestAges()
{
  const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.0, 0.0)), MakeSingles(), CheckOptions{});
  // Age = time since the latest activation at or before it: 0 ns and 100 ns.
  CHECK(result.perTime[0].ageNs == 1);    // t = 1
  CHECK(result.perTime[2].ageNs == 99);   // t = 99, still the first source
  CHECK(result.perTime[3].ageNs == 1);    // t = 101
  CHECK(result.perTime[5].ageNs == 100);  // t = 200
  CHECK(result.minAgeNs == 1 && result.maxAgeNs == 100);
}

void TestDisagreementFails()
{
  // A is 20% too high in the combined history.
  const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.2, 0.0)), MakeSingles(), CheckOptions{});
  CHECK(!result.Pass());
  CHECK(result.failed == kTimes.size());  // species A at every time, B untouched
  CHECK(result.perSpecies[0].failed == kTimes.size());
  CHECK(result.perSpecies[1].failed == 0);
  CHECK(result.perSpecies[0].worstRatio > 1);
  for (const auto& cell : result.cells)
    if (cell.species == "A") CHECK(cell.basis == Basis::Failed && !cell.pass);
}

void TestToleranceTerms()
{
  // B: constant in every seed (standard error 0), so only the relative term and
  // the floor can absorb a difference. Expected B is 1 before 101 ns and 3 later.
  {
    CheckOptions options;
    options.absoluteFloor = 0.5;  // a difference of 0.4 passes through the floor
    const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.0, 0.4)), MakeSingles(), options);
    CHECK(result.Pass());
    CHECK(result.byFloor > 0);
    const auto& b = result.cells[0 * 2 + 1];  // t = 1, E = 1: 5% = 0.05 < 0.4 <= 0.55
    CHECK(b.basis == Basis::Floor);
    CHECK(std::abs(b.tolerance - (0.0 + 0.05 * 1.0 + 0.5)) < 1e-12);
    CHECK(std::abs(b.ratio - 0.4 / 0.55) < 1e-12);
  }
  {
    CheckOptions strict;
    strict.absoluteFloor = 0.3;  // 0.4 > 0.05 + 0.3
    const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.0, 0.4)), MakeSingles(), strict);
    CHECK(!result.Pass());
  }
  {
    // 3% too high on a large constant count: the relative term decides.
    std::vector<SingleEnsemble> singles(1);
    singles[0].activationNs = 0;
    singles[0].tables = MakeEnsemble([](double, std::size_t, std::size_t) { return 1000.0; });
    const auto combined = MakeEnsemble([](double, std::size_t, std::size_t) { return 1030.0; });
    const auto result = CompareSeparated(combined, singles, CheckOptions{});
    CHECK(result.Pass());
    CHECK(result.cells[0].basis == Basis::Relative);
    CHECK(result.byRelative == result.cells.size());
    // 6% is out.
    const auto tooHigh = MakeEnsemble([](double, std::size_t, std::size_t) { return 1060.0; });
    CHECK(!CompareSeparated(tooHigh, singles, CheckOptions{}).Pass());
  }
}

void TestStatisticalTermUsesBothEnsembles()
{
  // Constant singles (SE 0) and a combined ensemble with zero-mean noise of
  // standard error s: a mean shift of 3.5 SE passes, 4.5 SE fails (no floor,
  // no relative term).
  std::vector<SingleEnsemble> singles(1);
  singles[0].activationNs = 0;
  singles[0].tables = MakeEnsemble([](double, std::size_t, std::size_t) { return 1000.0; });
  CheckOptions options;
  options.relativeTolerance = 0;
  options.absoluteFloor = 0;
  double sumSquares = 0;
  for (std::size_t seed = 0; seed < kSeeds; ++seed) sumSquares += Noise(seed) * Noise(seed);
  const double se = std::sqrt(sumSquares / (kSeeds - 1.0) / kSeeds);  // standard error of the noise
  CHECK(se > 0.5);
  for (const auto& [shiftInSe, pass] : {std::pair{3.5, true}, std::pair{4.5, false}})
  {
    const auto combined = MakeEnsemble([&](double, std::size_t, std::size_t seed) {
      return 1000.0 + shiftInSe * se + Noise(seed);
    });
    const auto result = CompareSeparated(combined, singles, options);
    CHECK(result.Pass() == pass);
    CHECK(std::abs(result.cells[0].combinedSe - se) < 1e-9);
    CHECK(result.cells[0].independentSe == 0);
  }
}

void TestInconsistentInputs()
{
  const auto combined = MakeEnsemble(CombinedCount(1.0, 0.0));
  const auto singles = MakeSingles();
  CHECK_THROWS(CompareSeparated({combined[0]}, singles, CheckOptions{}), "at least 2 seeds");
  CHECK_THROWS(CompareSeparated(combined, {}, CheckOptions{}), "no source");

  auto fewer = singles;
  fewer[1].tables.pop_back();
  CHECK_THROWS(CompareSeparated(combined, fewer, CheckOptions{}), "source 1 has 15 single histories");

  auto sameTime = singles;
  sameTime[1].activationNs = 0;
  CHECK_THROWS(CompareSeparated(combined, sameTime, CheckOptions{}), "share an activation time");

  auto otherTimes = combined;
  otherTimes[3].timesNs[2] = 98;
  CHECK_THROWS(CompareSeparated(otherTimes, singles, CheckOptions{}), "combined history 3: the record times differ");

  auto otherSpecies = singles;
  otherSpecies[0].tables[5].species[1] = "C";
  CHECK_THROWS(CompareSeparated(combined, otherSpecies, CheckOptions{}),
               "source 0, seed index 5: the species differ");

  // A single history that counts molecules before its source activates.
  auto early = singles;
  early[1].tables[2].counts[0] = 1;  // t = 1, species A, source 1 (activation 100)
  CHECK_THROWS(CompareSeparated(combined, early, CheckOptions{}), "nonzero count of A at 1 ns, before the activation");

  CheckOptions bad;
  bad.sigmas = 0;
  CHECK_THROWS(CompareSeparated(combined, singles, bad), "invalid tolerance options");
}

void TestReadSpeciesTable()
{
  std::istringstream good(
      "timeNs,species,count,history,chemistrySeed\n"
      "1,A,3,single,5\n"
      "1,\"B,x\",0,single,5\n"
      "2.5,A,4,single,5\r\n"
      "2.5,\"B,x\",1.5,single,5\n");
  const auto table = ReadSpeciesTable(good, "good");
  CHECK(table.timesNs.size() == 2 && table.timesNs[1] == 2.5);
  CHECK(table.species.size() == 2 && table.species[1] == "B,x");
  CHECK(table.At(0, 0) == 3 && table.At(0, 1) == 0 && table.At(1, 0) == 4 && table.At(1, 1) == 1.5);

  const auto read = [](const std::string& text) {
    std::istringstream in(text);
    return ReadSpeciesTable(in, "t");
  };
  CHECK_THROWS(read(""), "empty file");
  CHECK_THROWS(read("time,species,count\n"), "the header must start with");
  CHECK_THROWS(read("timeNs,species,count\n"), "no data rows");
  CHECK_THROWS(read("timeNs,species,count\n1,A,x\n"), "line 2: malformed number 'x'");
  CHECK_THROWS(read("timeNs,species,count\n1,A,-1\n"), "negative count");
  CHECK_THROWS(read("timeNs,species,count\n1,A,1\n1,A,2\n"), "species 'A' listed twice");
  CHECK_THROWS(read("timeNs,species,count\n2,A,1\n1,A,2\n"), "strictly increasing");
  CHECK_THROWS(read("timeNs,species,count\n1,A,1\n1,B,1\n2,A,1\n3,A,1\n3,B,1\n"), "differ from those at the first");
  CHECK_THROWS(read("timeNs,species,count\n1,A\n"), "at least 3 fields");
}

void TestOutputs()
{
  const auto result = CompareSeparated(MakeEnsemble(CombinedCount(1.0, 0.0)), MakeSingles(), CheckOptions{});
  std::ostringstream csv;
  WriteCellsCsv(csv, result);
  const std::string text = csv.str();
  CHECK(text.rfind("timeNs,ageNs,species,combinedMean,combinedSe,independentMean,independentSe,difference,se,"
                   "tolerance,ratio,basis,pass\n", 0) == 0);
  std::size_t lines = 0;
  for (const char c : text) lines += c == '\n';
  CHECK(lines == 1 + result.cells.size());

  std::ostringstream json;
  WriteJson(json, MakeSummaryJson(result));
  CHECK(json.str().find("\"pass\": true") != std::string::npos);
  CHECK(json.str().find("\"cells\": 12") != std::string::npos);

  std::ostringstream summary;
  WriteTextSummary(summary, result);
  CHECK(summary.str().find("SEPARATED_CHECK_PASS cells=12 failed=0") != std::string::npos);

  const auto failing = CompareSeparated(MakeEnsemble(CombinedCount(1.2, 0.0)), MakeSingles(), CheckOptions{});
  std::ostringstream failedSummary;
  WriteTextSummary(failedSummary, failing);
  CHECK(failedSummary.str().find("SEPARATED_CHECK_FAIL cells=12 failed=6") != std::string::npos);
}

}  // namespace

int main()
{
  TestAgreeingHistoriesPass();
  TestAges();
  TestDisagreementFails();
  TestToleranceTerms();
  TestStatisticalTermUsesBothEnsembles();
  TestInconsistentInputs();
  TestReadSpeciesTable();
  TestOutputs();
  std::puts("SeparatedCheckTest passed");
  return 0;
}
