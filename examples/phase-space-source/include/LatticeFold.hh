// LatticeFold: which box of the Scoring lattice a position belongs to, and the
// position in that box's frame (Geant4-free).
//
// The lattice is a square array of identical boxes of pitch P at the scoring
// depth: box (ix, iy) is centred at (ix P, iy P, zc), where zc is the z of the
// box centres in the phantom frame and the lattice axis is x = y = 0. Box
// (ix, iy) owns the lateral cell [(ix - 1/2) P, (ix + 1/2) P) x [(iy - 1/2) P,
// (iy + 1/2) P): a position exactly on a cell edge belongs to the box on its +
// side, so local x and y are always in [-P/2, P/2).
//
// The box frame has its origin at the box centre and the phantom's axes:
// local = (x - ix P, y - iy P, z - zc). P = 0 is the single box: (0, 0) and
// local = (x, y, z - zc) for any position.
//
// Any length unit, the same for every argument. The fold says nothing about
// whether the position is inside the box (it is the stage-1 geometry's job to
// record only inward crossings).
#pragma once

#include <array>
#include <cstdint>

namespace phsp
{

struct LatticeSite
{
  std::int64_t ix = 0;
  std::int64_t iy = 0;
  std::array<double, 3> local = {0, 0, 0};
  bool operator==(const LatticeSite&) const = default;
};

class LatticeFold
{
public:
  // Throws std::runtime_error when pitch < 0 or a value is not finite.
  LatticeFold(double pitch, double boxCentreZ);

  double Pitch() const { return fPitch; }
  double BoxCentreZ() const { return fBoxCentreZ; }

  // Throws std::runtime_error on a non-finite position or a box index that
  // does not fit in 62 bits.
  LatticeSite Fold(const std::array<double, 3>& position) const;
  // Centre of box (ix, iy) in the phantom frame.
  std::array<double, 3> BoxCentre(std::int64_t ix, std::int64_t iy) const;

private:
  double fPitch;
  double fBoxCentreZ;
};

}  // namespace phsp
