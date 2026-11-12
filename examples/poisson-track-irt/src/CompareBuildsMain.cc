// compare-builds: stock-vs-patched comparison of finished histories (Geant4-free).
//
//   compare-builds --stock <dir> --patched <dir> --out <csv>
//
// Each directory holds one subdirectory per history, named c<cluster>-s<seed>
// (for example c03-s11), each with the species.csv written by `history`. The
// histories of the two builds are paired by (cluster, seed); both directories
// must hold the same pairs. The method, the margin and the verdicts are in
// include/BuildComparison.hh.
//
// Writes the per-(time, species) table to <csv> and prints the verdict counts.
// It reports only: the exit status is 0 whatever the verdicts (EQUIVALENT,
// DIFFERENT, INCONCLUSIVE, EMPTY and the bias flags are data, not a pass/fail),
// 2 with "COMPARE_BUILDS_ERROR <reason>" for bad arguments or inputs.
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "BuildComparison.hh"

namespace
{

class InputError : public std::runtime_error
{
 public:
  using std::runtime_error::runtime_error;
};

std::vector<ptirt::BuildHistory> LoadBuild(const std::filesystem::path& root, const char* build)
{
  if (!std::filesystem::is_directory(root))
    throw InputError(std::string(build) + " directory not found: " + root.string());
  static const std::regex name(R"(c(\d+)-s(\d+))");
  std::vector<ptirt::BuildHistory> histories;
  for (const auto& entry : std::filesystem::directory_iterator(root))
  {
    if (!entry.is_directory()) continue;
    std::smatch match;
    const std::string folder = entry.path().filename().string();
    if (!std::regex_match(folder, match, name)) continue;
    const auto file = entry.path() / "species.csv";
    std::ifstream in(file, std::ios::binary);
    if (!in) throw InputError("cannot open " + file.string());
    ptirt::BuildHistory history;
    history.cluster = static_cast<std::size_t>(std::stoull(match[1].str()));
    history.seed = static_cast<std::size_t>(std::stoull(match[2].str()));
    try
    {
      history.table = ptirt::ReadSpeciesTable(in, file.string());
    }
    catch (const std::runtime_error& error)
    {
      throw InputError(error.what());
    }
    histories.push_back(std::move(history));
  }
  if (histories.empty())
    throw InputError(std::string(build) + " directory holds no c<cluster>-s<seed>/species.csv: " + root.string());
  return histories;
}

int Run(int argc, char** argv)
{
  std::filesystem::path stockDir, patchedDir, outFile;
  for (int i = 1; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (i + 1 >= argc) throw InputError("missing value after " + option);
    const std::string value = argv[++i];
    if (option == "--stock")
      stockDir = value;
    else if (option == "--patched")
      patchedDir = value;
    else if (option == "--out")
      outFile = value;
    else
      throw InputError("unknown option " + option);
  }
  if (stockDir.empty() || patchedDir.empty() || outFile.empty())
    throw InputError("usage: compare-builds --stock <dir> --patched <dir> --out <csv>");

  const auto stock = LoadBuild(stockDir, "stock");
  const auto patched = LoadBuild(patchedDir, "patched");
  std::vector<ptirt::ComparisonRow> rows;
  try
  {
    rows = ptirt::CompareBuilds(stock, patched);
  }
  catch (const std::runtime_error& error)
  {
    throw InputError(error.what());
  }

  if (outFile.has_parent_path()) std::filesystem::create_directories(outFile.parent_path());
  {
    std::ofstream out(outFile, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open " + outFile.string());
    ptirt::WriteComparisonCsv(out, rows);
    out.close();
    if (!out) throw std::runtime_error("cannot write " + outFile.string());
  }

  const auto summary = ptirt::Summarize(rows);
  std::size_t clusters = rows.empty() ? 0 : rows.front().clusters;
  std::cout << "COMPARE_BUILDS rows=" << summary.Total() << " clusters=" << clusters
            << " histories=" << stock.size() << '\n'
            << "verdicts EQUIVALENT=" << summary.equivalent << " DIFFERENT=" << summary.different
            << " INCONCLUSIVE=" << summary.inconclusive << " EMPTY=" << summary.empty << '\n'
            << "biasFlags=" << summary.biasFlags << " biasFlagsEquivalent=" << summary.biasEquivalent << '\n'
            << "csv " << outFile.string() << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  try
  {
    return Run(argc, argv);
  }
  catch (const InputError& error)
  {
    std::cerr << "COMPARE_BUILDS_ERROR " << error.what() << std::endl;
    return 2;
  }
  catch (const std::exception& error)
  {
    std::cerr << "COMPARE_BUILDS_ERROR " << error.what() << std::endl;
    return 1;
  }
}
