// TimeGrid: the record times and the cumulative deposited energy of a
// dose-cutoff history (Geant4-free, covered by test/TimeGridTest.cc).
//
// Common grid. Every dose-cutoff history records the species counts at the
// same absolute times, whatever its dose rate: t_k = startNs * 10^(k / ppd),
// k = 0, 1, ... (1 ps and 10 points per decade in `history`), the beam-on
// start being t = 0. Each point is computed from its own k, never by
// accumulation, so two histories share every point below the smaller of their
// beam-off times bit for bit. The grid stops below beam-off and beam-off itself
// is appended as the last point.
//
// Age grids. For arrival j (arrival time a_j), the ages startAgeNs *
// 10^(k / ppd), k = 0, 1, ..., as absolute times a_j + age, while that time is
// below the next arrival time (or below the end, for the last arrival): the
// counts against the age since each arrival, between it and the next one.
//
// Cumulative energy. A source (one injected packet) enters the history at its
// activation time (arrival + capture age); from then on its molecules are in
// the species counts and its deposited energy in E(t). The molecule counter of
// `history` merges times closer than kCounterPrecisionNs, so a source counts at
// a record time t when activation - t < kCounterPrecisionNs; CumulativeEnergy
// uses the same rule, so N(t) and E(t) always describe the same sources.
#pragma once

#include <cstddef>
#include <vector>

namespace drs
{

// Fixed precision of the history's G4MoleculeCounter time comparer (1 fs).
inline constexpr double kCounterPrecisionNs = 1.0e-6;
// The common grid of every dose-cutoff history: from 1 ps, 10 points per decade.
inline constexpr double kCommonGridStartNs = 1.0e-3;
inline constexpr int kCommonGridPointsPerDecade = 10;
// Beam-off (the chemistry end) lies this long after the cutoff source's
// activation, so its molecules join the history before it ends.
inline constexpr double kBeamOffMarginNs = 1.0e-3;
// A snapshot time this close to an activation or to the end is taken at that
// stage boundary (ActivationObserver.hh); a watched time is never closer than
// this to the next delayed list. The same value as the scheduler's 1 ps step tolerance.
inline constexpr double kSnapshotGuardNs = 1.0e-3;
inline constexpr std::size_t kMaxTimeGridPoints = 5'000'000;

// startNs * 10^(k / pointsPerDecade) for k = 0, 1, ... strictly below endNs,
// then endNs. A point within a relative 1e-12 of endNs is dropped (endNs takes
// its place). Throws std::invalid_argument on a non-positive or non-finite
// start or end, end < start, or pointsPerDecade outside [1, 100].
std::vector<double> BuildCommonLogGrid(double startNs, int pointsPerDecade, double endNs);

// The snapshot times: the points of the common grid closest to
// startNs * 10^(s / snapshotsPerDecade), s = 0, 1, ..., plus the last point
// (beam-off). `grid` is a BuildCommonLogGrid result made with
// gridPointsPerDecade; the selection is a subset of it (increasing, no
// repeats), so a snapshot time is a record time of species.csv bit for bit.
// Throws std::invalid_argument for an empty grid or a snapshot density
// outside [1, gridPointsPerDecade].
std::vector<double> SelectSnapshotTimes(const std::vector<double>& grid, int gridPointsPerDecade,
                                        int snapshotsPerDecade);

struct AgePoint
{
  std::size_t arrival = 0;  // position in the arrival list
  double ageNs = 0;         // time since the arrival
  double timeNs = 0;        // arrival time + age
  bool operator==(const AgePoint&) const = default;
};

// For each arrival j: arrivalTimesNs[j] + startAgeNs * 10^(k / pointsPerDecade)
// while below min(arrivalTimesNs[j + 1], endNs) (endNs for the last arrival).
// Arrival order, then increasing age. Arrival times must be finite,
// nondecreasing and below endNs. Throws std::invalid_argument on invalid input
// or more than kMaxTimeGridPoints points.
std::vector<AgePoint> BuildAgeGrids(const std::vector<double>& arrivalTimesNs, double startAgeNs,
                                    int pointsPerDecade, double endNs);

// E(t): the summed deposits of the sources that are in the history at t.
class CumulativeEnergy
{
 public:
  // activationTimesNs nondecreasing and finite, depositsEv finite and >= 0,
  // same length. Throws std::invalid_argument otherwise.
  CumulativeEnergy(std::vector<double> activationTimesNs, const std::vector<double>& depositsEv,
                   double precisionNs = kCounterPrecisionNs);

  // Sum of the deposits of the sources with activation - timeNs < precision.
  double At(double timeNs) const;
  // Number of those sources.
  std::size_t SourcesAt(double timeNs) const;
  double Total() const { return fPrefix.back(); }
  std::size_t Size() const { return fActivationNs.size(); }

 private:
  std::vector<double> fActivationNs;
  std::vector<double> fPrefix;  // fPrefix[n] = sum of the first n deposits
  double fPrecisionNs;
};

}  // namespace drs
