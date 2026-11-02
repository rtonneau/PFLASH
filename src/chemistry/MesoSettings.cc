/// \file MesoSettings.cc
/// Computes mesh sizes and record times from mesoscopic-stage settings.
/// Dependencies: Standard library only.
/// Thread safety: Helpers use local state; write Current() only before concurrent readers start.
#include "chemistry/MesoSettings.hh"

#include <cmath>
#include <stdexcept>

namespace MesoSettings
{
namespace
{
constexpr int kMaxExponent = 30;
constexpr double kEndTimeRelativeMargin = 1e-9;

int UncappedPixelCount(double boxSide, double targetVoxel, int maxPixels)
{
  if (!(boxSide > 0.) || !(targetVoxel > 0.) || maxPixels < 1) {
    throw std::invalid_argument("MesoSettings::PixelCount: box side, voxel size and cap must be > 0");
  }
  long long best = 1;
  double bestCellSizeDifference = std::abs(boxSide - targetVoxel);
  for (int k = 1; k <= kMaxExponent; ++k) {
    const long long pixelCount = 1LL << k;
    const double cellSizeDifference = std::abs(boxSide / static_cast<double>(pixelCount) - targetVoxel);
    if (cellSizeDifference <= bestCellSizeDifference) {  // ties favor more pixels
      best = pixelCount;
      bestCellSizeDifference = cellSizeDifference;
    }
  }
  return static_cast<int>(best);
}
}  // namespace

int PixelCount(double boxSide, double targetVoxel, int maxPixels)
{
  const int pixelCount = UncappedPixelCount(boxSide, targetVoxel, maxPixels);
  return pixelCount < maxPixels ? pixelCount : maxPixels;
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
    const double recordTime = start * std::pow(10., static_cast<double>(k) / perDecade);
    // Relative margin so that start * 10^n landing a rounding error below end
    // does not leave two near-identical last values.
    if (recordTime >= end * (1. - kEndTimeRelativeMargin)) break;
    grid.push_back(recordTime);
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
