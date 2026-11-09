// SeparatedCheck: the comparison of the separated-sources check (Geant4-free).
//
// Question. Sources far apart cannot influence each other, so ONE history that
// receives several precursor packets at different times must equal, within
// statistics, the sum of the same packets evolved ALONE:
//
//   combined(t, species)  ~  sum over sources k of single_k(t, species)
//
// where single_k is the history of source k alone, activated at the same
// absolute time t_k as in the combined history, so single_k(t) is the count at
// molecular age t - t_k, and is exactly 0 before t_k. All histories are
// recorded on the same absolute record grid (HistoryConfig.hh: single mode
// keeps the grid of the full schedule), so no interpolation is needed.
//
// Ensembles. N combined histories (chemistry seeds c_1..c_N) and, for each
// source k, N single histories (seeds s_k,1..s_k,N), all with different seeds;
// the packets and placements are the same in all of them, so only the
// chemistry differs. The independent expectation of seed index i is the sum
// over k of single_k,i, which gives N independent values of the expected count.
//
// Cell test, for each record time t and species:
//   C, SE_C   mean over the N combined histories and its standard error
//             (sample standard deviation / sqrt(N), denominator N - 1);
//   E, SE_E   the same for the N summed single histories;
//   SE        sqrt(SE_C^2 + SE_E^2);
//   pass      |C - E| <= sigmas * SE + relativeTolerance * E + absoluteFloor.
// Defaults: sigmas 4, relativeTolerance 0.05, absoluteFloor 0 (the rule
// |C - E| <= 4 SE + 5% exactly). A floor (e.g. 0.5 molecule, --floor) is an
// optional allowance for species with a mean of a few molecules,
// where the sample standard error of 16 seeds is not reliable (for example all
// seeds equal, SE = 0); every cell records which term decided it (Basis).
// The tolerance is never changed by the code to make a check pass.
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "Ledger.hh"

namespace ptirt
{

// Molecule counts per record time and species of one history (species.csv).
struct SpeciesTable
{
  std::vector<double> timesNs;      // strictly increasing
  std::vector<std::string> species; // the same species at every time
  std::vector<double> counts;       // [timeIndex * species.size() + speciesIndex]

  double At(std::size_t timeIndex, std::size_t speciesIndex) const
  {
    return counts[timeIndex * species.size() + speciesIndex];
  }
};

// Reads species.csv written by `history` (header timeNs,species,count[,history,
// chemistrySeed]; rows grouped by time). Throws std::runtime_error naming the
// label and line on a malformed file: wrong header, bad numbers, negative or
// non-finite count, times not strictly increasing, a species repeated within a
// time, or a time that lists other species than the first.
SpeciesTable ReadSpeciesTable(std::istream& in, const std::string& label);

struct CheckOptions
{
  double sigmas = 4.0;
  double relativeTolerance = 0.05;
  double absoluteFloor = 0;  // molecules
};

// The single-source histories of one source: its activation time and one table
// per seed index.
struct SingleEnsemble
{
  double activationNs = 0;  // scheduled time of the source
  std::vector<SpeciesTable> tables;
};

// Which term of the tolerance decided a passing cell.
enum class Basis { Statistical, Relative, Floor, Failed };
const char* BasisName(Basis basis);

struct CellResult
{
  std::size_t timeIndex = 0;
  std::size_t speciesIndex = 0;
  double timeNs = 0;
  double ageNs = 0;  // since the latest activation at or before timeNs
  std::string species;
  double combinedMean = 0, combinedSe = 0;
  double independentMean = 0, independentSe = 0;
  double difference = 0;  // combinedMean - independentMean
  double se = 0;          // combined standard error of the difference
  double tolerance = 0;
  double ratio = 0;       // |difference| / tolerance (0 when both are 0)
  Basis basis = Basis::Statistical;
  bool pass = true;
};

// One entry per record time / per species: cells, failures, worst ratio.
struct GroupSummary
{
  std::string label;  // "t=<timeNs> ns" or the species name
  double timeNs = 0;
  double ageNs = 0;
  std::size_t cells = 0;
  std::size_t failed = 0;
  std::size_t byStatistics = 0, byRelative = 0, byFloor = 0;
  double worstRatio = 0;
  std::string worstWhere;  // species (per time) or time (per species)
};

struct CheckResult
{
  CheckOptions options;
  std::size_t combinedSeeds = 0;
  std::size_t singleSeeds = 0;  // per source
  std::size_t sources = 0;
  std::vector<double> activationsNs;
  std::vector<CellResult> cells;       // time-major
  std::vector<GroupSummary> perTime;
  std::vector<GroupSummary> perSpecies;
  std::size_t failed = 0;
  std::size_t byStatistics = 0, byRelative = 0, byFloor = 0;
  double minAgeNs = 0, maxAgeNs = 0;
  bool Pass() const { return failed == 0 && !cells.empty(); }
};

// Compares the combined ensemble with the single-source ensembles. Throws
// std::runtime_error when the inputs are inconsistent: fewer than 2 seeds, an
// ensemble size that differs from the others, different record times or species
// in any table, a single history with a nonzero count before its activation,
// or no source.
CheckResult CompareSeparated(const std::vector<SpeciesTable>& combined,
                             const std::vector<SingleEnsemble>& singles, const CheckOptions& options);

// timeNs,ageNs,species,combinedMean,combinedSe,independentMean,independentSe,
// difference,se,tolerance,ratio,basis,pass  (17 significant digits).
void WriteCellsCsv(std::ostream& out, const CheckResult& result);

// The check as a JsonValue tree: tolerance, inputs, totals, per-species and per-time summaries (no cells).
JsonValue MakeSummaryJson(const CheckResult& result);

// Human-readable summary: inputs, tolerance, per-species table, the worst
// cells, the per-time block for every record time, and the
// final line "SEPARATED_CHECK_PASS ..." or "SEPARATED_CHECK_FAIL ...".
void WriteTextSummary(std::ostream& out, const CheckResult& result, std::size_t worstCellCount = 10);

}  // namespace ptirt
