/// \file MesoSettings.cc
#include "chemistry/MesoSettings.hh"

#include <cmath>
#include <stdexcept>

namespace MesoSettings
{
namespace
{
constexpr int kMaxExponent = 30;

int UncappedPixelCount(double boxSide, double targetVoxel, int maxPixels)
{
  if (!(boxSide > 0.) || !(targetVoxel > 0.) || maxPixels < 1) {
    throw std::invalid_argument("MesoSettings::PixelCount: box side, voxel size and cap must be > 0");
  }
  long long best = 1;
  double bestDiff = std::abs(boxSide - targetVoxel);
  for (int k = 1; k <= kMaxExponent; ++k) {
    const long long p = 1LL << k;
    const double diff = std::abs(boxSide / static_cast<double>(p) - targetVoxel);
    if (diff <= bestDiff) {  // <=: ties go to the larger p
      best = p;
      bestDiff = diff;
    }
  }
  return static_cast<int>(best);
}
}  // namespace

int PixelCount(double boxSide, double targetVoxel, int maxPixels)
{
  const int p = UncappedPixelCount(boxSide, targetVoxel, maxPixels);
  return p < maxPixels ? p : maxPixels;
}

bool PixelCountCapped(double boxSide, double targetVoxel, int maxPixels)
{
  return UncappedPixelCount(boxSide, targetVoxel, maxPixels) > maxPixels;
}

std::vector<double> LogTimeGrid(double start, double end, int perDecade)
{
  if (!(start > 0.) || !(start < end) || perDecade < 1) {
    throw std::invalid_argument("MesoSettings::LogTimeGrid: need 0 < start < end and perDecade >= 1");
  }
  std::vector<double> grid{start};
  for (int k = 1;; ++k) {
    const double t = start * std::pow(10., static_cast<double>(k) / perDecade);
    // Relative margin so that start * 10^n landing a rounding error below end
    // does not leave two near-identical last values.
    if (t >= end * (1. - 1e-9)) break;
    grid.push_back(t);
  }
  grid.push_back(end);
  return grid;
}

Values& Current()
{
  static Values values;
  return values;
}

bool StageEnabled(const Values& values, bool sbsModel)
{
  return values.enabled && !sbsModel;
}

bool ExplicitlyEnabledWithSbs(const Values& values, bool sbsModel)
{
  return sbsModel && values.enabled && values.enabledExplicitly;
}
}  // namespace MesoSettings
