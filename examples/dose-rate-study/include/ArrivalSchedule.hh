// ArrivalSchedule: when and where each precursor packet is injected.
//
// An Arrival says "packet number packetIndex of the library is injected at
// timeNs, rigidly translated by offsetNm". Three ways to make a schedule:
//   1. ParseDeterministicSchedule: read a small text file.
//   2. SamplePoissonArrivals: draw a Poisson process from a source seed.
//   3. SampleDoseCutoffArrivals: a Poisson process at the rate of a dose rate,
//      stopped when the deposited energy reaches a dose (ComputeArrivalRates).
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

namespace drs
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

// ---- Dose cutoff ---------------------------------------------------------------
//
// Rates. A dose rate Ddot (Gy/s) in a water box of mass m is reached by
// launching primaries at lambda_launched = Ddot * m / ebar_launched, where
// ebar_launched is the mean box deposit per LAUNCHED primary. Some launched
// primaries leave no molecule in the box ("empty" crossings: `capture` makes no
// packet for them, CaptureLibrary.hh). They are handled by thinning: an empty
// crossing has no chemistry, so it is not scheduled at all, and its deposit is
// counted as 0 (the deposits `capture` records for them are reported, not
// used). The arrival process of the packets is then the launched process
// thinned by the hit fraction f = packets / launched:
//   lambda_packet = f * lambda_launched = Ddot * m / ebar_packet,
// ebar_packet being the mean deposit per packet. Both rates are kept.
//
// Box mass m = (2 * halfSide)^3 * 1000 kg/m^3; for the 2 um half side (4 um
// box) m = 6.4e-14 kg, so 1 Gy is 6.4e-14 J = 3.99e5 eV.
inline constexpr double kWaterDensityKgPerM3 = 1000.0;

struct ArrivalRates
{
  double doseRateGyPerS = 0;
  double targetDoseGy = 0;
  double boxHalfSideNm = 0;
  double boxMassKg = 0;
  double targetEnergyEv = 0;           // targetDoseGy * boxMassKg / e
  std::size_t packetCount = 0;         // library packets (box crossings with molecules)
  std::size_t emptyCount = 0;          // launched primaries without a packet
  std::size_t launchedCount = 0;       // packetCount + emptyCount
  double hitFraction = 0;              // packetCount / launchedCount
  double packetDepositSumEv = 0;
  double emptyDepositSumEv = 0;        // reported only: the empties count as 0
  double meanDepositPerLaunchedEv = 0;  // ebar_launched = packetDepositSumEv / launchedCount
  double meanDepositPerPacketEv = 0;    // ebar_packet = packetDepositSumEv / packetCount
  double launchedRateHz = 0;           // Ddot * m / ebar_launched
  double packetRateHz = 0;             // hitFraction * launchedRateHz, the scheduled rate
};

// Throws std::runtime_error on a non-positive or non-finite dose rate, dose or
// half side, no packet, a negative or non-finite deposit, or a zero deposit sum.
ArrivalRates ComputeArrivalRates(double doseRateGyPerS, double doseGy, double boxHalfSideNm,
                                 const std::vector<double>& packetDepositsEv, std::size_t emptyCount,
                                 double emptyDepositSumEv);

struct DoseCutoffParameters
{
  double rateHz = 0;          // packet arrival rate (ArrivalRates::packetRateHz), > 0
  double targetEnergyEv = 0;  // > 0
  std::uint64_t sourceSeed = 1;
  std::size_t firstPacket = 0;  // arrival k uses packet firstPacket + k
};

struct DoseCutoffSchedule
{
  std::vector<Arrival> arrivals;            // offsets 0: the packets are box-centred
  std::vector<double> cumulativeEnergyEv;   // after each arrival (E_k = sum of deposits 0..k)
};

// The first arrival is at t = 0 (no draw); then one std::mt19937_64(sourceSeed)
// uniform per wait, built as in SamplePoissonArrivals (wait = -log1p(-u) /
// rate). Arrivals are added until the cumulative deposit reaches the target:
// the last arrival K is the first with E_K >= target, so E_{K-1} < target <= E_K
// and the overshoot E_K - target is less than one track's deposit. Arrival k
// uses packet firstPacket + k (never reused). Throws std::runtime_error on
// invalid parameters, "library exhausted" (the packets run out before the
// target) or "censored" (more than maxArrivals arrivals).
DoseCutoffSchedule SampleDoseCutoffArrivals(const DoseCutoffParameters& parameters,
                                            const std::vector<double>& packetDepositsEv, std::size_t maxArrivals);

// Energy checks of a dose-cutoff history.
struct DoseEnergyChecks
{
  bool targetReached = false;            // E_final >= target
  bool overshootBelowOneTrack = false;   // E_final - (last deposit) < target
  bool beamOffHoldsEverySource = false;  // E(beam-off) == E_final
  bool injectedMatchesSchedule = false;  // deposits of the injected sources sum to E_final
  bool AllPassed() const
  {
    return targetReached && overshootBelowOneTrack && beamOffHoldsEverySource && injectedMatchesSchedule;
  }
};

// The two sums are compared with a relative tolerance of 1e-12 (they add the
// same deposits, possibly in another order).
DoseEnergyChecks CheckDoseEnergy(const DoseCutoffSchedule& schedule, double targetEnergyEv,
                                 double energyAtBeamOffEv, double injectedDepositEv);

}  // namespace drs
