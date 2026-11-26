// Lattice fold: box index and box-frame position, on and around cell edges,
// and the single box (pitch 0).
#include "LatticeFold.hh"

#include <cmath>
#include <limits>

#include "TestCheck.hh"

using namespace phsp;

namespace
{

bool Site(const LatticeSite& s, std::int64_t ix, std::int64_t iy, double x, double y, double z)
{
  return s.ix == ix && s.iy == iy && s.local[0] == x && s.local[1] == y && s.local[2] == z;
}

}  // namespace

int main()
{
  const double depth = 1.0e6;
  const LatticeFold fold(100.0, depth);

  // Inside the central cell.
  CHECK(Site(fold.Fold({0, 0, depth}), 0, 0, 0, 0, 0));
  CHECK(Site(fold.Fold({12.5, -30, depth + 2}), 0, 0, 12.5, -30, 2));

  // Cell edges belong to the box on the + side: local x, y in [-P/2, P/2).
  CHECK(Site(fold.Fold({50, 0, depth}), 1, 0, -50, 0, 0));
  CHECK(Site(fold.Fold({-50, 0, depth}), 0, 0, -50, 0, 0));
  CHECK(Site(fold.Fold({150, -150, depth}), 2, -1, -50, -50, 0));
  CHECK(Site(fold.Fold({0, 50, depth - 2000}), 0, 1, 0, -50, -2000));

  // Just either side of an edge.
  const double below = std::nextafter(50.0, 0.0);
  const LatticeSite s1 = fold.Fold({below, 0, depth});
  CHECK(s1.ix == 0 && s1.local[0] > 49.99 && s1.local[0] < 50);
  const double above = std::nextafter(50.0, 100.0);
  const LatticeSite s2 = fold.Fold({above, 0, depth});
  CHECK(s2.ix == 1 && s2.local[0] >= -50 && s2.local[0] < -49.99);

  // Far boxes, negative side.
  CHECK(Site(fold.Fold({-1234.0, 9876.0, depth}), -12, 99, -34, -24, 0));

  // A pitch whose quotient rounds: the local coordinate stays in [-P/2, P/2).
  const LatticeFold small(0.2, 0);
  for (double x : {0.1, 0.3, 0.5, -0.1, -0.3, 0.7000000000000001})
  {
    const LatticeSite s = small.Fold({x, x, 0});
    CHECK(s.local[0] >= -0.1 && s.local[0] < 0.1);
    CHECK(std::abs(s.ix * 0.2 + s.local[0] - x) < 1e-15);
  }

  // Box centres.
  const auto centre = fold.BoxCentre(-3, 2);
  CHECK(centre[0] == -300 && centre[1] == 200 && centre[2] == depth);

  // Pitch 0: a single box on the axis, for any lateral position.
  const LatticeFold single(0, depth);
  CHECK(Site(single.Fold({50, -7000, depth + 3}), 0, 0, 50, -7000, 3));
  CHECK(Site(single.Fold({1e9, 1e9, depth}), 0, 0, 1e9, 1e9, 0));

  // Errors.
  CHECK_THROWS(LatticeFold(-1, 0), "pitch must be finite and >= 0");
  CHECK_THROWS(LatticeFold(std::numeric_limits<double>::infinity(), 0), "pitch");
  CHECK_THROWS(LatticeFold(1, std::numeric_limits<double>::quiet_NaN()), "box centre z");
  CHECK_THROWS(fold.Fold({std::numeric_limits<double>::quiet_NaN(), 0, 0}), "not finite");
  CHECK_THROWS(LatticeFold(1e-300, 0).Fold({1e10, 0, 0}), "too far");
  return 0;
}
