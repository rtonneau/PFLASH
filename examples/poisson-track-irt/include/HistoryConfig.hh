// HistoryConfig: the input of one `history` run, read from a small text file,
// and the absolute-time record grid of species.csv (Geant4-free).
//
// One shared IRT_syn chemistry history receives several precursor packets
// (captured separately by `capture`), each injected at its own arrival time.
// The config file says which packets, when, and how the history is observed.
//
// File format: one `key = value` per line; '#' at the start of a line (after
// blanks) starts a comment; blank lines are ignored. Every key at most once;
// an unknown key, a key that the selected mode does not use, an empty value or
// a malformed number is an error naming the line.
//
//   mode               deterministic | single | poisson                (required)
//   libraryDir         packet library written by `capture`              (required)
//   endpointNs         end of the chemistry history, ns, > 0            (required)
//   chemistrySeed      seed of the chemistry history, 1..2147483647     (required)
//   scheduleFile       deterministic schedule (ArrivalSchedule.hh)      (deterministic, single)
//   singleSource       index of the one schedule row to inject alone    (single)
//   rateHz             mean arrival rate, 1/s, > 0                      (poisson)
//   exposureS          arrivals fall in [0, exposureS), s, > 0          (poisson)
//   sourceSeed         seed of the arrival sampler, 1..2147483647       (poisson)
//   regionCenterUm     x y z of the placement box centre, um            (poisson, default 0 0 0)
//   regionHalfSizeUm   half extent, one value or x y z, um, >= 0        (poisson, default 50)
//   worldHalfSideUm    half side of the water world, um, > 0            (default 1000)
//   halfBoxUm          reflecting chemistry box half side, um; 0 = none (default 0)
//                      if > 0 it must be smaller than worldHalfSideUm
//   gridMinAgeNs       first record age after each arrival, ns, >= 0.01 (default 1)
//   gridPointsPerDecade record points per decade of age, 1..100         (default 5)
//   newOldContactLimit more than this many new-old contact pairs in the whole history
//                      (ActivationObserver::NewOldContacts) end the run with exit
//                      status 1 after the outputs are written; integer >= 0
//                      (default: no limit; 0 allows none)
//
// Relative paths are relative to the directory of the config file. The
// chemistry seed and the source seed are separate on purpose: the same arrival
// schedule can be replayed with another chemistry seed, and vice versa.
//
// "single" injects one row of the deterministic schedule, alone, at its
// scheduled time and offset: the control history of that source. It keeps the
// record grid of the full schedule, so its species.csv lines up with the
// deterministic history built from the same file.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "ArrivalSchedule.hh"

namespace ptirt
{

enum class HistoryMode { Deterministic, Single, Poisson };

const char* HistoryModeName(HistoryMode mode);

inline constexpr std::uint64_t kMaxHistorySeed = 2147483647;  // G4Random::setTheSeed takes a 32-bit long on Windows

struct HistoryConfig
{
  HistoryMode mode = HistoryMode::Deterministic;
  std::filesystem::path libraryDir;
  std::filesystem::path scheduleFile;  // deterministic and single only
  std::size_t singleSource = 0;        // single only
  double endpointNs = 0;
  std::uint64_t chemistrySeed = 0;
  PoissonParameters poisson;           // poisson only (rate, exposure, region, sourceSeed)
  double worldHalfSideNm = 1.0e6;
  double halfBoxNm = 0;                // 0 = unbounded
  double gridMinAgeNs = 1;
  int gridPointsPerDecade = 5;
  std::optional<std::uint64_t> newOldContactLimit;  // unset = no limit
};

// Throws std::runtime_error ("line N: ...") on any violation. Relative paths
// are resolved against baseDirectory.
HistoryConfig ReadHistoryConfig(std::istream& in, const std::filesystem::path& baseDirectory);

// Opens and reads a config file; relative paths are resolved against its directory.
HistoryConfig ReadHistoryConfigFile(const std::filesystem::path& file);

// Record grid guard: no record time is closer than this to an arrival time, so
// a grid point never lands on the step where a new source is counted (the
// source is counted at arrival + 1 ps, and the molecule counter merges times
// within 1 ps).
inline constexpr double kGridGuardNs = 5.0e-3;
inline constexpr std::size_t kMaxGridPoints = 1'000'000;

// Absolute record times, ns, increasing: for each anchor a in {0} and the
// arrival times, a + minAgeNs * 10^(k / pointsPerDecade) for k = 0, 1, ...
// while below endpointNs; points within kGridGuardNs of an arrival time are
// dropped; the endpoint is always the last point. Throws on invalid arguments
// or more than kMaxGridPoints points.
std::vector<double> BuildRecordGrid(const std::vector<double>& arrivalTimesNs, double endpointNs,
                                    double minAgeNs, int pointsPerDecade);

}  // namespace ptirt
