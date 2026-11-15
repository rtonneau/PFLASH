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
//   mode               deterministic | single | poisson | doseCutoff   (required)
//   libraryDir         packet library written by `capture`              (required)
//   endpointNs         end of the chemistry history, ns, > 0            (required; not doseCutoff)
//   chemistrySeed      seed of the chemistry history, 1..2147483647     (required)
//   scheduleFile       deterministic schedule (ArrivalSchedule.hh)      (deterministic, single)
//   singleSource       index of the one schedule row to inject alone    (single)
//   rateHz             mean arrival rate, 1/s, > 0                      (poisson)
//   exposureS          arrivals fall in [0, exposureS), s, > 0          (poisson)
//   sourceSeed         seed of the arrival sampler, 1..2147483647       (poisson, doseCutoff)
//   regionCenterUm     x y z of the placement box centre, um            (poisson, default 0 0 0)
//   regionHalfSizeUm   half extent, one value or x y z, um, >= 0        (poisson, default 50)
//   worldHalfSideUm    half side of the water world, um, > 0            (default 1000)
//   halfBoxUm          reflecting chemistry box half side, um; 0 = none (default 0; doseCutoff:
//                      if > 0 it must be smaller than worldHalfSideUm    required, > 0)
//   gridMinAgeNs       first record age after each arrival, ns, >= 0.01 (default 1; not doseCutoff)
//   gridPointsPerDecade record points per decade of age, 1..100         (default 5; not doseCutoff)
//   progressIntervalS  wall seconds between HISTORY_PROGRESS console    (default 0 = off)
//                      lines during the chemistry, >= 0
//
// doseCutoff only (ArrivalSchedule.hh "Dose cutoff", TimeGrid.hh, SteadyStateDetector.hh):
//   doseRateGyPerS     dose rate in the capture box, Gy/s, > 0          (required)
//   doseGy             dose at which the beam stops, Gy, > 0            (default 1)
//   firstPacket        library packet of the first arrival, >= 0        (default 0)
//   maxArrivals        arrival cap, 1..10000000 (exceeding it is fatal) (default 100000)
//   steadyStateWindow  arrivals per steady-state window; 0 = no stop    (default 50)
//   steadyStateTolerance relative window-to-window change, > 0          (default 0.05)
//   steadyStateMinMean species with both window means <= this are      (default 0)
//                      not compared, >= 0
//   ageGridPointsPerDecade age-grid points per decade, 0..100; 0 = no  (default 5)
//                      species_age.csv rows
// The doseCutoff history has no endpointNs: it ends at beam-off, just after
// the arrival that reaches doseGy (or earlier, at a steady-state stop). Its
// record times are the common log grid from 1 ps, 10 per decade (TimeGrid.hh),
// plus the age grids from 1 ps after each arrival. halfBoxUm must be the
// capture box of the library (checked by `history`): the dose is the energy
// deposited in that box divided by its water mass.
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
#include <string>
#include <vector>

#include "ArrivalSchedule.hh"
#include "SteadyStateDetector.hh"

namespace drs
{

enum class HistoryMode { Deterministic, Single, Poisson, DoseCutoff };

const char* HistoryModeName(HistoryMode mode);

inline constexpr std::uint64_t kMaxHistorySeed = 2147483647;  // G4Random::setTheSeed takes a 32-bit long on Windows

struct DoseCutoffConfig
{
  double doseRateGyPerS = 0;
  double doseGy = 1;
  std::uint64_t sourceSeed = 0;
  std::size_t firstPacket = 0;
  std::size_t maxArrivals = 100000;
  std::size_t steadyStateWindow = 50;  // 0 = no steady-state stop
  SteadyStateSettings steadyState;      // window = steadyStateWindow when it is > 0
  int ageGridPointsPerDecade = 5;       // 0 = no age grid
};

struct HistoryConfig
{
  HistoryMode mode = HistoryMode::Deterministic;
  std::filesystem::path libraryDir;
  std::filesystem::path scheduleFile;  // deterministic and single only
  std::size_t singleSource = 0;        // single only
  double endpointNs = 0;               // doseCutoff: 0 here, beam-off is set by `history`
  std::uint64_t chemistrySeed = 0;
  PoissonParameters poisson;           // poisson only (rate, exposure, region, sourceSeed)
  double worldHalfSideNm = 1.0e6;
  double halfBoxNm = 0;                // 0 = unbounded
  double gridMinAgeNs = 1;
  int gridPointsPerDecade = 5;
  double progressIntervalS = 0;        // 0 = no progress lines
  DoseCutoffConfig dose;               // doseCutoff only
};

// Throws std::runtime_error ("line N: ...") on any violation. Relative paths
// are resolved against baseDirectory.
HistoryConfig ReadHistoryConfig(std::istream& in, const std::filesystem::path& baseDirectory);

// Opens and reads a config file; relative paths are resolved against its directory.
HistoryConfig ReadHistoryConfigFile(const std::filesystem::path& file);

// Record grid guard: no record time is closer than this to an arrival time, so
// a grid point never lands on the step where a new source is counted (the
// source is counted at its activation, and the molecule counter merges times
// within kCounterPrecisionNs = 1 fs, TimeGrid.hh). Not used in doseCutoff mode,
// whose record times include the arrivals' own grids.
inline constexpr double kGridGuardNs = 5.0e-3;
inline constexpr std::size_t kMaxGridPoints = 1'000'000;

// Absolute record times, ns, increasing: for each anchor a in {0} and the
// arrival times, a + minAgeNs * 10^(k / pointsPerDecade) for k = 0, 1, ...
// while below endpointNs; points within kGridGuardNs of an arrival time are
// dropped; the endpoint is always the last point. Throws on invalid arguments
// or more than kMaxGridPoints points.
std::vector<double> BuildRecordGrid(const std::vector<double>& arrivalTimesNs, double endpointNs,
                                    double minAgeNs, int pointsPerDecade);

}  // namespace drs
