// ArrivalSchedule: when and where each precursor packet is injected.
//
// An Arrival says "packet number packetIndex of the library is injected at
// timeNs, rigidly translated by offsetNm". Two ways to make a schedule:
//   1. ParseDeterministicSchedule: read a small text file.
//   2. SamplePoissonArrivals: draw a Poisson process from a source seed.
//
// Geant4-free. Units in this header: time ns (rates and exposure in the
// Poisson parameters are in s and 1/s, as named), length nm.
//
// Deterministic file format (one record per line; '#' starts a comment line,
// blank lines are ignored):
//   # time_ns packet_index dx_um dy_um dz_um
//   0      0   0    0   0
//   500000 1   100  0   0
// Columns: arrival time in ns, library packet index (0-based, integer), and the
// translation in micrometres (converted to nm in Arrival::offsetNm).
// Rules: all numbers finite; time >= 0 and < endpointNs; times nondecreasing;
// packet index < library size; each packet index used at most once (a packet
// is never reused); count between minArrivals and maxArrivals.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace ptirt
{

struct Arrival
{
  std::size_t sourceIndex = 0;  // 0,1,2... in time order
  double timeNs = 0;            // injection time of the packet (its molecules are at the capture age)
  std::size_t packetIndex = 0;  // index into the packet library
  std::array<double, 3> offsetNm{};
  bool operator==(const Arrival&) const = default;
};

// Bounds shared by both schedule makers. All explicit and configurable.
struct ScheduleBounds
{
  std::size_t libraryPacketCount = 0;  // packet indices must be < this
  double endpointNs = 0;               // observation end; arrivals must be earlier (> 0)
  std::size_t minArrivals = 1;         // deterministic files: at least this many records
  std::size_t maxArrivals = 100000;    // safety cap; exceeding it is an error, never truncation
};

// Throws std::runtime_error with the line number on any violation.
std::vector<Arrival> ParseDeterministicSchedule(std::istream& in, const ScheduleBounds& bounds);

struct PoissonParameters
{
  double rateHz = 2000.0;       // mean arrivals per second, > 0
  double exposureS = 2.0e-3;    // arrivals fall in the half-open window [0, exposureS), > 0
  // Placement region: a box centred at regionCenterNm with half extents
  // regionHalfSizeNm per axis (full side = 2 * half). A zero half extent pins
  // that axis to the centre. Offsets are uniform in the box.
  std::array<double, 3> regionCenterNm{0, 0, 0};
  std::array<double, 3> regionHalfSizeNm{50000, 50000, 50000};  // 100 um side
  std::uint64_t sourceSeed = 1;
};

// Poisson process via independent exponential waits, simulated with one
// std::mt19937_64(sourceSeed) stream. Draw order, for each arrival: one uniform
// for the wait, then three uniforms for x, y, z (the wait that first reaches
// the exposure end is drawn and discarded, so the stream is used identically
// for a given seed regardless of platform: uniforms are built from raw 64-bit
// output, u = (bits >> 11) * 2^-53, wait = -log1p(-u) / rate).
// Arrival k uses packet k (next unused packet; no reuse, no resampling, no
// conditioning on the number of arrivals). Zero arrivals is a valid result.
//
// Errors (std::runtime_error): invalid parameters; more arrivals than
// bounds.libraryPacketCount ("library exhausted"); more than bounds.maxArrivals
// ("censored": the sampler stops with an error instead of truncating).
// bounds.minArrivals is ignored here.
std::vector<Arrival> SamplePoissonArrivals(const PoissonParameters& parameters,
                                           const ScheduleBounds& bounds);

}  // namespace ptirt
