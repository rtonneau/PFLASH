// compare: the separated-sources check on finished histories (Geant4-free).
//
//   compare --inputs <manifest> --output <dir> [--sigmas <x>] [--relative <x>] [--floor <x>]
//
// The manifest lists the species.csv of every history (one per line, '#'
// starts a comment line):
//   combined  -             <species.csv>   one per combined history (seed)
//   single    <activationNs> <species.csv>  one per single-source history; the
//                                           lines of one activation time are
//                                           the seeds of that source, in order
// Relative paths are relative to the manifest's directory. See SeparatedCheck.hh
// for the test. Writes cells.csv, summary.json and summary.txt into <dir>
// (created if missing) and prints the summary. Exit status: 0 and a
// "SEPARATED_CHECK_PASS" line when every cell passes; 1 and
// "SEPARATED_CHECK_FAIL" when a cell fails; 2 and "COMPARE_ERROR <reason>" for
// bad arguments or inputs.
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Ledger.hh"
#include "SeparatedCheck.hh"

namespace
{

class InputError : public std::runtime_error
{
 public:
  using std::runtime_error::runtime_error;
};

double ParseOption(const std::string& option, const std::string& text)
{
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (text.empty() || end != text.c_str() + text.size() || !std::isfinite(value) || value < 0)
    throw InputError(option + " needs a finite number >= 0");
  return value;
}

ptirt::SpeciesTable LoadTable(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary);
  if (!in) throw InputError("cannot open " + file.string());
  try
  {
    return ptirt::ReadSpeciesTable(in, file.string());
  }
  catch (const std::runtime_error& error)
  {
    throw InputError(error.what());
  }
}

struct Inputs
{
  std::vector<ptirt::SpeciesTable> combined;
  std::vector<ptirt::SingleEnsemble> singles;  // in order of first appearance
};

Inputs ReadManifest(const std::filesystem::path& manifest)
{
  std::ifstream in(manifest);
  if (!in) throw InputError("cannot open the manifest " + manifest.string());
  auto base = manifest.parent_path();
  if (base.empty()) base = ".";

  Inputs inputs;
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(in, line))
  {
    ++lineNumber;
    std::istringstream fields(line);
    std::string kind;
    if (!(fields >> kind) || kind[0] == '#') continue;
    const std::string where = manifest.filename().string() + " line " + std::to_string(lineNumber) + ": ";
    std::string activation, path, extra;
    if (!(fields >> activation >> path) || (fields >> extra))
      throw InputError(where + "expected '<kind> <activationNs|-> <path>'");
    const std::filesystem::path file = std::filesystem::path(path).is_absolute()
                                           ? std::filesystem::path(path)
                                           : (base / path).lexically_normal();
    if (kind == "combined")
    {
      if (activation != "-") throw InputError(where + "a combined history has '-' as its activation");
      inputs.combined.push_back(LoadTable(file));
    }
    else if (kind == "single")
    {
      char* end = nullptr;
      const double time = std::strtod(activation.c_str(), &end);
      if (end != activation.c_str() + activation.size() || !std::isfinite(time))
        throw InputError(where + "malformed activation time '" + activation + "'");
      ptirt::SingleEnsemble* ensemble = nullptr;
      for (auto& candidate : inputs.singles)
        if (candidate.activationNs == time) ensemble = &candidate;
      if (!ensemble)
      {
        inputs.singles.emplace_back();
        ensemble = &inputs.singles.back();
        ensemble->activationNs = time;
      }
      ensemble->tables.push_back(LoadTable(file));
    }
    else
      throw InputError(where + "unknown kind '" + kind + "'");
  }
  return inputs;
}

template <typename Writer>
void WriteFile(const std::filesystem::path& path, Writer write)
{
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot open " + path.string());
  write(out);
  out.close();
  if (!out) throw std::runtime_error("cannot write " + path.string());
}

int Run(int argc, char** argv)
{
  std::filesystem::path manifest, output;
  ptirt::CheckOptions options;
  for (int i = 1; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (i + 1 >= argc) throw InputError("missing value after " + option);
    const std::string value = argv[++i];
    if (option == "--inputs")
      manifest = value;
    else if (option == "--output")
      output = value;
    else if (option == "--sigmas")
      options.sigmas = ParseOption(option, value);
    else if (option == "--relative")
      options.relativeTolerance = ParseOption(option, value);
    else if (option == "--floor")
      options.absoluteFloor = ParseOption(option, value);
    else
      throw InputError("unknown option " + option);
  }
  if (manifest.empty() || output.empty())
    throw InputError("usage: compare --inputs <manifest> --output <dir> [--sigmas x] [--relative x] [--floor x]");
  if (options.sigmas <= 0) throw InputError("--sigmas must be positive");

  const auto inputs = ReadManifest(manifest);
  ptirt::CheckResult result;
  try
  {
    result = ptirt::CompareSeparated(inputs.combined, inputs.singles, options);
  }
  catch (const std::runtime_error& error)
  {
    throw InputError(error.what());
  }

  std::filesystem::create_directories(output);
  WriteFile(output / "cells.csv", [&](std::ostream& out) { ptirt::WriteCellsCsv(out, result); });
  WriteFile(output / "summary.json", [&](std::ostream& out) {
    ptirt::WriteJson(out, ptirt::MakeSummaryJson(result));
    out << '\n';
  });
  WriteFile(output / "summary.txt", [&](std::ostream& out) { ptirt::WriteTextSummary(out, result); });
  ptirt::WriteTextSummary(std::cout, result);
  std::cout.flush();
  return result.Pass() ? 0 : 1;
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
    std::cerr << "COMPARE_ERROR " << error.what() << std::endl;
    return 2;
  }
  catch (const std::exception& error)
  {
    std::cerr << "COMPARE_ERROR " << error.what() << std::endl;
    return 1;
  }
}
