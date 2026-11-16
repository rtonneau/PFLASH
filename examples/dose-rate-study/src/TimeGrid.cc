#include "TimeGrid.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace drs
{
namespace
{

void Require(bool ok, const std::string& message)
{
  if (!ok) throw std::invalid_argument("TimeGrid: " + message);
}

void CheckDensity(int pointsPerDecade)
{
  Require(pointsPerDecade >= 1 && pointsPerDecade <= 100, "points per decade must be in [1, 100]");
}

double LogPoint(double startNs, int pointsPerDecade, int k)
{
  return startNs * std::pow(10.0, static_cast<double>(k) / pointsPerDecade);
}

}  // namespace

std::vector<double> BuildCommonLogGrid(double startNs, int pointsPerDecade, double endNs)
{
  Require(std::isfinite(startNs) && startNs > 0, "the grid start must be positive and finite");
  Require(std::isfinite(endNs) && endNs >= startNs, "the grid end must be finite and not below the start");
  CheckDensity(pointsPerDecade);
  std::vector<double> grid;
  for (int k = 0;; ++k)
  {
    const double t = LogPoint(startNs, pointsPerDecade, k);
    if (!(t < endNs * (1 - 1e-12))) break;
    grid.push_back(t);
    Require(grid.size() < kMaxTimeGridPoints, "more than " + std::to_string(kMaxTimeGridPoints) + " points");
  }
  grid.push_back(endNs);
  return grid;
}

std::vector<double> SelectSnapshotTimes(const std::vector<double>& grid, int gridPointsPerDecade,
                                        int snapshotsPerDecade)
{
  Require(!grid.empty(), "the grid to select snapshot times from is empty");
  CheckDensity(gridPointsPerDecade);
  Require(snapshotsPerDecade >= 1 && snapshotsPerDecade <= gridPointsPerDecade,
          "snapshots per decade must be in [1, the grid's points per decade]");
  std::vector<std::size_t> indices;
  // The last grid point is the appended end; the regular points are before it.
  for (std::size_t s = 0;; ++s)
  {
    const auto index = static_cast<std::size_t>(
        std::llround(static_cast<double>(s) * gridPointsPerDecade / snapshotsPerDecade));
    if (index + 1 >= grid.size()) break;
    if (indices.empty() || index > indices.back()) indices.push_back(index);
  }
  std::vector<double> times;
  times.reserve(indices.size() + 1);
  for (const auto index : indices) times.push_back(grid[index]);
  times.push_back(grid.back());
  return times;
}

std::vector<AgePoint> BuildAgeGrids(const std::vector<double>& arrivalTimesNs, double startAgeNs,
                                    int pointsPerDecade, double endNs)
{
  Require(std::isfinite(startAgeNs) && startAgeNs > 0, "the first age must be positive and finite");
  Require(std::isfinite(endNs) && endNs > 0, "the end must be positive and finite");
  CheckDensity(pointsPerDecade);
  for (std::size_t j = 0; j < arrivalTimesNs.size(); ++j)
  {
    const double t = arrivalTimesNs[j];
    Require(std::isfinite(t) && t >= 0 && t < endNs, "an arrival time is not in [0, end)");
    Require(j == 0 || t >= arrivalTimesNs[j - 1], "the arrival times decrease");
  }

  std::vector<AgePoint> points;
  for (std::size_t j = 0; j < arrivalTimesNs.size(); ++j)
  {
    const double arrival = arrivalTimesNs[j];
    const double limit = j + 1 < arrivalTimesNs.size() ? std::min(arrivalTimesNs[j + 1], endNs) : endNs;
    for (int k = 0;; ++k)
    {
      const double age = LogPoint(startAgeNs, pointsPerDecade, k);
      const double t = arrival + age;
      if (!(t < limit)) break;
      points.push_back(AgePoint{j, age, t});
      Require(points.size() < kMaxTimeGridPoints, "more than " + std::to_string(kMaxTimeGridPoints) +
                                                      " age points; lower ageGridPointsPerDecade");
    }
  }
  return points;
}

CumulativeEnergy::CumulativeEnergy(std::vector<double> activationTimesNs, const std::vector<double>& depositsEv,
                                   double precisionNs)
    : fActivationNs(std::move(activationTimesNs)), fPrecisionNs(precisionNs)
{
  Require(fActivationNs.size() == depositsEv.size(), "one deposit per activation time is needed");
  Require(std::isfinite(precisionNs) && precisionNs >= 0, "the precision must be finite and not negative");
  fPrefix.assign(1, 0.0);
  for (std::size_t i = 0; i < fActivationNs.size(); ++i)
  {
    Require(std::isfinite(fActivationNs[i]), "an activation time is not finite");
    Require(i == 0 || fActivationNs[i] >= fActivationNs[i - 1], "the activation times decrease");
    Require(std::isfinite(depositsEv[i]) && depositsEv[i] >= 0, "a deposit is negative or not finite");
    fPrefix.push_back(fPrefix.back() + depositsEv[i]);
  }
}

std::size_t CumulativeEnergy::SourcesAt(double timeNs) const
{
  // First activation a with a - t >= precision, i.e. a >= t + precision.
  const double bound = timeNs + fPrecisionNs;
  return static_cast<std::size_t>(std::lower_bound(fActivationNs.begin(), fActivationNs.end(), bound) -
                                  fActivationNs.begin());
}

double CumulativeEnergy::At(double timeNs) const
{
  return fPrefix[SourcesAt(timeNs)];
}

}  // namespace drs
