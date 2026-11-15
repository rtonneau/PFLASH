// snapshots-check: validate the snapshots.h5 a doseCutoff `history` wrote
// (needs HDF5).
//
//   snapshots-check --snapshots <snapshots.h5> --species <species.csv>
//                   [--activations <activations.csv>] [--box-half-side-nm <h>]
//
// Requires:
//   - format version 1, a species table that holds every species of species.csv;
//   - at least one frame, strictly increasing frame times, the last one at
//     beam-off (the last time of species.csv);
//   - the frames are the contiguous tail of the selected snapshot times (3 per
//     decade of the common grid up to beam-off, SelectSnapshotTimes): only the
//     times before the first activation have no frame, and each frame time is
//     within kSnapshotGuardNs of its selected time;
//   - every position finite, every species id inside the table, and, with
//     --box-half-side-nm, every position inside the reflecting box;
//   - every frame whose time is a time of species.csv (not an extrapolated
//     row) holds, per species, the count of that row; except, with --activations,
//     a frame within a few picoseconds after an activation, taken while the
//     pre-chemical molecules of that activation are still dissociating.
// Prints "SNAPSHOTS_CHECK_OK frames=... molecules=... matchedSpecies=..." and
// exits 0, or "SNAPSHOTS_CHECK_ERROR <reason>" and exits 1.
#include <algorithm>
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

#include "SnapshotWriter.hh"
#include "TimeGrid.hh"

namespace
{

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error(message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

// species.csv: timeNs -> species -> count, for the rows that are not extrapolated.
struct SpeciesCsv
{
  std::map<double, std::map<std::string, double>> counts;
  std::vector<std::string> species;
  double lastTimeNs = 0;
};

SpeciesCsv ReadSpeciesCsv(const std::filesystem::path& file)
{
  std::ifstream in(file);
  Require(static_cast<bool>(in), "cannot open " + file.string());
  std::string line;
  std::getline(in, line);
  Require(line.rfind("timeNs,species,count", 0) == 0, "species.csv: unexpected header " + line);
  SpeciesCsv csv;
  std::map<std::string, bool> seen;
  while (std::getline(in, line))
  {
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::istringstream row(line);
    for (std::string field; std::getline(row, field, ',');) fields.push_back(field);
    Require(fields.size() >= 8, "species.csv: short row " + line);
    const double time = std::strtod(fields[0].c_str(), nullptr);
    csv.lastTimeNs = std::max(csv.lastTimeNs, time);
    if (!seen[fields[1]])
    {
      seen[fields[1]] = true;
      csv.species.push_back(fields[1]);
    }
    if (fields[7] == "0") csv.counts[time][fields[1]] = std::strtod(fields[2].c_str(), nullptr);
  }
  Require(!csv.counts.empty(), "species.csv has no row");
  return csv;
}

// observedActivationTimeNs (column 8) of every row of activations.csv.
std::vector<double> ReadActivationTimes(const std::filesystem::path& file)
{
  std::ifstream in(file);
  Require(static_cast<bool>(in), "cannot open " + file.string());
  std::string line;
  std::getline(in, line);
  std::vector<double> times;
  while (std::getline(in, line))
  {
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::istringstream row(line);
    for (std::string field; std::getline(row, field, ',');) fields.push_back(field);
    Require(fields.size() >= 8, "activations.csv: short row " + line);
    times.push_back(std::strtod(fields[7].c_str(), nullptr));
  }
  return times;
}

int Run(int argc, char** argv)
{
  std::filesystem::path snapshots, speciesFile, activationsFile;
  double halfBoxNm = 0;
  for (int i = 1; i < argc; i += 2)
  {
    const std::string option = argv[i];
    Require(i + 1 < argc, "missing value after " + option);
    if (option == "--snapshots")
      snapshots = argv[i + 1];
    else if (option == "--species")
      speciesFile = argv[i + 1];
    else if (option == "--activations")
      activationsFile = argv[i + 1];
    else if (option == "--box-half-side-nm")
      halfBoxNm = std::stod(argv[i + 1]);
    else
      Fail("unknown option " + option);
  }
  Require(!snapshots.empty() && !speciesFile.empty(), "usage: snapshots-check --snapshots <h5> --species <csv>");

  const auto csv = ReadSpeciesCsv(speciesFile);
  const auto activations = activationsFile.empty() ? std::vector<double>() : ReadActivationTimes(activationsFile);
  const auto file = drs::ReadSnapshotFile(snapshots);
  Require(file.formatVersion == drs::kSnapshotFormatVersion, "wrong format version");
  Require(!file.frames.empty(), "no frame");
  for (const auto& name : csv.species)
    Require(std::find(file.species.begin(), file.species.end(), name) != file.species.end(),
            "species " + name + " of species.csv is not in the species table");

  // The selected times; a frame stands for the selected times within tolerance of
  // its time (times of the early picoseconds share a frame, a frame is taken at
  // the first step end after its time). The frames cover the selected times from
  // the first frame on, in order; the times before it precede the first activation.
  const double tolerance = 5 * drs::kSnapshotGuardNs;
  const auto grid = drs::BuildCommonLogGrid(drs::kCommonGridStartNs, drs::kCommonGridPointsPerDecade, csv.lastTimeNs);
  const auto selected = drs::SelectSnapshotTimes(grid, drs::kCommonGridPointsPerDecade, 3);
  std::size_t nextSelected = 0;
  while (nextSelected < selected.size() && selected[nextSelected] < file.frames.front().timeNs - tolerance)
    ++nextSelected;

  std::size_t molecules = 0, matched = 0;
  double previous = 0;
  for (std::size_t f = 0; f < file.frames.size(); ++f)
  {
    const auto& frame = file.frames[f];
    const std::string at = "frame " + std::to_string(f) + " (" + std::to_string(frame.timeNs) + " ns): ";
    Require(frame.timeNs > previous, at + "time does not increase");
    previous = frame.timeNs;
    Require(nextSelected < selected.size(), at + "no selected time left");
    Require(std::abs(frame.timeNs - selected[nextSelected]) <= tolerance,
            at + "not the selected time " + std::to_string(selected[nextSelected]));
    ++nextSelected;
    while (nextSelected < selected.size() && std::abs(frame.timeNs - selected[nextSelected]) <= tolerance &&
           (f + 1 == file.frames.size() || std::abs(file.frames[f + 1].timeNs - selected[nextSelected]) > tolerance))
      ++nextSelected;  // merged into this frame
    Require(frame.positionsNm.size() == 3 * frame.species.size(), at + "positions and species differ in size");
    std::vector<double> counts(file.species.size(), 0.0);
    for (std::size_t m = 0; m < frame.species.size(); ++m)
    {
      Require(frame.species[m] < file.species.size(), at + "species id outside the table");
      counts[frame.species[m]] += 1;
      for (int axis = 0; axis < 3; ++axis)
      {
        const double x = frame.positionsNm[3 * m + axis];
        Require(std::isfinite(x), at + "a position is not finite");
        if (halfBoxNm > 0) Require(std::abs(x) <= halfBoxNm * (1 + 1e-6), at + "a molecule lies outside the box");
      }
    }
    molecules += frame.species.size();

    bool joining = false;  // within a few ps after an activation
    for (const double a : activations)
      if (frame.timeNs >= a && frame.timeNs - a <= tolerance) joining = true;
    const auto row = csv.counts.find(frame.timeNs);
    if (row != csv.counts.end() && !joining)
    {
      ++matched;
      for (std::size_t s = 0; s < file.species.size(); ++s)
      {
        const auto count = row->second.find(file.species[s]);
        const double expected = count != row->second.end() ? count->second : 0.0;
        Require(counts[s] == expected, at + "species " + file.species[s] + " has " + std::to_string(counts[s]) +
                                           " molecules, species.csv has " + std::to_string(expected));
      }
    }
  }
  Require(std::abs(file.frames.back().timeNs - csv.lastTimeNs) <= drs::kSnapshotGuardNs,
          "the last frame is not at beam-off");
  Require(nextSelected == selected.size(), "some selected snapshot times have no frame");
  Require(matched >= 1, "no frame time is a time of species.csv: the counts were not compared");

  std::cout << "SNAPSHOTS_CHECK_OK frames=" << file.frames.size() << " selected=" << selected.size()
            << " molecules=" << molecules << " countsMatched=" << matched << " species=" << file.species.size()
            << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  try
  {
    return Run(argc, argv);
  }
  catch (const std::exception& error)
  {
    std::cerr << "SNAPSHOTS_CHECK_ERROR " << error.what() << std::endl;
    return 1;
  }
}
