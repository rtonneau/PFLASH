#include "LatticeFold.hh"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace phsp
{

namespace
{

constexpr double kMaxCellIndex = 4611686018427387904.0;  // 2^62

// Cell index and offset from the cell centre along one lateral axis, with the
// offset in [-pitch/2, pitch/2) even after rounding.
std::pair<std::int64_t, double> FoldAxis(double value, double pitch)
{
  const double cell = std::floor(value / pitch + 0.5);
  if (!(std::abs(cell) < kMaxCellIndex))
    throw std::runtime_error("lattice fold: position too far from the lattice axis for pitch " +
                             std::to_string(pitch));
  auto index = static_cast<std::int64_t>(cell);
  double local = value - cell * pitch;
  if (local >= 0.5 * pitch)
  {
    ++index;
    local -= pitch;
  }
  else if (local < -0.5 * pitch)
  {
    --index;
    local += pitch;
  }
  return {index, local};
}

}  // namespace

LatticeFold::LatticeFold(double pitch, double boxCentreZ) : fPitch(pitch), fBoxCentreZ(boxCentreZ)
{
  if (!std::isfinite(pitch) || pitch < 0)
    throw std::runtime_error("lattice fold: pitch must be finite and >= 0 (0 = single box)");
  if (!std::isfinite(boxCentreZ))
    throw std::runtime_error("lattice fold: box centre z must be finite");
}

LatticeSite LatticeFold::Fold(const std::array<double, 3>& position) const
{
  for (double value : position)
    if (!std::isfinite(value))
      throw std::runtime_error("lattice fold: position is not finite");
  LatticeSite site;
  site.local = {position[0], position[1], position[2] - fBoxCentreZ};
  if (fPitch == 0)
    return site;
  const auto [ix, localX] = FoldAxis(position[0], fPitch);
  const auto [iy, localY] = FoldAxis(position[1], fPitch);
  site.ix = ix;
  site.iy = iy;
  site.local[0] = localX;
  site.local[1] = localY;
  return site;
}

std::array<double, 3> LatticeFold::BoxCentre(std::int64_t ix, std::int64_t iy) const
{
  return {static_cast<double>(ix) * fPitch, static_cast<double>(iy) * fPitch, fBoxCentreZ};
}

}  // namespace phsp
