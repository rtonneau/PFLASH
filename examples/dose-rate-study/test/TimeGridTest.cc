#include "TimeGrid.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "TestCheck.hh"

using namespace drs;

namespace
{

bool Near(double a, double b, double relative = 1e-12)
{
  return std::abs(a - b) <= relative * std::max(std::abs(a), std::abs(b));
}

void TestCommonGrid()
{
  // 1 ps to 1 ns at 10 points per decade: 30 points below 1 ns, then 1 ns.
  const auto grid = BuildCommonLogGrid(kCommonGridStartNs, kCommonGridPointsPerDecade, 1.0);
  CHECK(grid.size() == 31);
  CHECK(grid.front() == kCommonGridStartNs);
  CHECK(grid.back() == 1.0);
  CHECK(Near(grid[10], 1e-2));
  CHECK(Near(grid[20], 1e-1));
  for (std::size_t k = 1; k < grid.size(); ++k) CHECK(grid[k] > grid[k - 1]);

  // The end is appended: the last regular point is strictly below it.
  const auto odd = BuildCommonLogGrid(1e-3, 10, 0.5);
  CHECK(odd.back() == 0.5);
  CHECK(odd[odd.size() - 2] < 0.5);
  CHECK(Near(odd[odd.size() - 2], 1e-3 * std::pow(10.0, 26.0 / 10)));

  // A start equal to the end is a one-point grid.
  const auto single = BuildCommonLogGrid(1e-3, 10, 1e-3);
  CHECK(single.size() == 1 && single[0] == 1e-3);
}

void TestCommonGridIsSharedAcrossEnds()
{
  // Two histories with different beam-off times share every point below the
  // smaller one, bit for bit: the grid does not depend on the rate.
  const auto shortGrid = BuildCommonLogGrid(kCommonGridStartNs, kCommonGridPointsPerDecade, 123.456);
  const auto longGrid = BuildCommonLogGrid(kCommonGridStartNs, kCommonGridPointsPerDecade, 9.87e6);
  CHECK(shortGrid.size() < longGrid.size());
  for (std::size_t k = 0; k + 1 < shortGrid.size(); ++k) CHECK(shortGrid[k] == longGrid[k]);
  CHECK(shortGrid.back() == 123.456);
  CHECK(longGrid.back() == 9.87e6);
}

void TestCommonGridRejects()
{
  CHECK_THROWS(BuildCommonLogGrid(0, 10, 1), "start");
  CHECK_THROWS(BuildCommonLogGrid(-1, 10, 1), "start");
  CHECK_THROWS(BuildCommonLogGrid(1e-3, 10, 1e-4), "end");
  CHECK_THROWS(BuildCommonLogGrid(1e-3, 10, INFINITY), "end");
  CHECK_THROWS(BuildCommonLogGrid(1e-3, 0, 1), "points per decade");
  CHECK_THROWS(BuildCommonLogGrid(1e-3, 101, 1), "points per decade");
}

void TestAgeGrids()
{
  // Arrivals at 0, 0.05 and 1 ns, end 2 ns, ages from 1 ps at 1 point per decade.
  const auto points = BuildAgeGrids({0.0, 0.05, 1.0}, 1e-3, 1, 2.0);
  // Arrival 0: ages 1e-3, 1e-2 (0.1 >= 0.05). Arrival 1: 0.051, 0.06, 0.15
  // (1.05 >= 1). Arrival 2: 1.001, 1.01, 1.1 (2.0 >= 2).
  CHECK(points.size() == 8);
  CHECK((points[0] == AgePoint{0, 1e-3, 1e-3}));
  CHECK(points[1].arrival == 0 && Near(points[1].ageNs, 1e-2));
  CHECK(points[2].arrival == 1 && points[2].ageNs == 1e-3 && points[2].timeNs == 0.05 + 1e-3);
  CHECK(points[4].arrival == 1 && Near(points[4].ageNs, 0.1));
  CHECK(points[5].arrival == 2 && points[5].timeNs == 1.0 + 1e-3);
  CHECK(points[7].arrival == 2 && Near(points[7].ageNs, 0.1));
  for (const auto& p : points) CHECK(p.timeNs < 2.0);

  // Two arrivals at the same time: the first has no point before the second.
  const auto same = BuildAgeGrids({0.0, 0.0}, 1e-3, 1, 1e-2);
  CHECK(same.size() == 1 && same[0].arrival == 1);

  CHECK(BuildAgeGrids({}, 1e-3, 5, 1).empty());
}

void TestAgeGridRejects()
{
  CHECK_THROWS(BuildAgeGrids({0.0}, 0, 5, 1), "first age");
  CHECK_THROWS(BuildAgeGrids({0.0}, 1e-3, 5, 0), "end");
  CHECK_THROWS(BuildAgeGrids({0.0}, 1e-3, 0, 1), "points per decade");
  CHECK_THROWS(BuildAgeGrids({1.0, 0.5}, 1e-3, 5, 2), "decrease");
  CHECK_THROWS(BuildAgeGrids({0.0, 2.0}, 1e-3, 5, 2), "[0, end)");
  CHECK_THROWS(BuildAgeGrids({-1.0}, 1e-3, 5, 2), "[0, end)");
}

void TestCumulativeEnergy()
{
  const CumulativeEnergy energy({0.001, 10.0, 10.0, 20.0}, {100, 5, 7, 50});
  CHECK(energy.Size() == 4);
  CHECK(energy.Total() == 162);
  CHECK(energy.At(0) == 0);
  CHECK(energy.SourcesAt(0) == 0);
  // The counter precision: a source counts at t when activation - t < 1 fs.
  CHECK(energy.At(0.001) == 100);
  CHECK(energy.At(0.001 - 0.5e-6) == 100);
  CHECK(energy.At(0.001 - 2e-6) == 0);
  CHECK(energy.At(9.999) == 100);
  CHECK(energy.At(10.0) == 112);
  CHECK(energy.SourcesAt(10.0) == 3);
  CHECK(energy.At(19.0) == 112);
  CHECK(energy.At(1e9) == 162);

  // Precision 0: a source counts only strictly after its activation.
  const CumulativeEnergy exact({1.0}, {3}, 0);
  CHECK(exact.At(1.0) == 0);
  CHECK(exact.At(std::nextafter(1.0, 2.0)) == 3);

  const CumulativeEnergy empty({}, {});
  CHECK(empty.Total() == 0 && empty.At(5) == 0);
}

void TestCumulativeEnergyRejects()
{
  CHECK_THROWS(CumulativeEnergy({1.0}, {}), "one deposit");
  CHECK_THROWS(CumulativeEnergy({2.0, 1.0}, {1, 1}), "decrease");
  CHECK_THROWS(CumulativeEnergy({1.0}, {-1}), "deposit");
  CHECK_THROWS(CumulativeEnergy({NAN}, {1}), "not finite");
  CHECK_THROWS(CumulativeEnergy({1.0}, {1}, -1), "precision");
}

void TestSnapshotTimes()
{
  // 1 ps to 1 ns: 30 regular points then 1 ns. Three per decade takes the
  // grid points 0, 3, 7, 10, 13, 17, 20, 23, 27 (nearest to k * 10 / 3), then 1 ns.
  const auto grid = BuildCommonLogGrid(kCommonGridStartNs, kCommonGridPointsPerDecade, 1.0);
  const auto times = SelectSnapshotTimes(grid, kCommonGridPointsPerDecade, 3);
  const std::vector<std::size_t> expected{0, 3, 7, 10, 13, 17, 20, 23, 27};
  CHECK(times.size() == expected.size() + 1);
  for (std::size_t k = 0; k < expected.size(); ++k) CHECK(times[k] == grid[expected[k]]);
  CHECK(times.back() == 1.0);
  for (std::size_t k = 1; k < times.size(); ++k) CHECK(times[k] > times[k - 1]);

  // The full density is the whole grid; one per decade keeps the decades.
  CHECK(SelectSnapshotTimes(grid, 10, 10) == grid);
  const auto decades = SelectSnapshotTimes(grid, 10, 1);
  CHECK(decades.size() == 4);
  CHECK(decades[1] == grid[10] && decades[2] == grid[20] && decades.back() == 1.0);

  // A one-point grid is its own selection.
  const auto single = BuildCommonLogGrid(1e-3, 10, 1e-3);
  CHECK(SelectSnapshotTimes(single, 10, 3) == single);

  CHECK_THROWS(SelectSnapshotTimes({}, 10, 3), "empty");
  CHECK_THROWS(SelectSnapshotTimes(grid, 10, 0), "snapshots per decade");
  CHECK_THROWS(SelectSnapshotTimes(grid, 10, 11), "snapshots per decade");
}

}  // namespace

int main()
{
  TestSnapshotTimes();
  TestCommonGrid();
  TestCommonGridIsSharedAcrossEnds();
  TestCommonGridRejects();
  TestAgeGrids();
  TestAgeGridRejects();
  TestCumulativeEnergy();
  TestCumulativeEnergyRejects();
  return 0;
}
