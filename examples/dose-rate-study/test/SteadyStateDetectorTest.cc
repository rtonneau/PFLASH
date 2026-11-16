#include "SteadyStateDetector.hh"

#include <cmath>
#include <stdexcept>

#include "TestCheck.hh"

using namespace drs;

namespace
{

SteadyStateSettings Settings(std::size_t window, double tolerance, double minMean = 0)
{
  SteadyStateSettings s;
  s.window = window;
  s.tolerance = tolerance;
  s.minMean = minMean;
  return s;
}

void TestDefaults()
{
  const SteadyStateSettings s;
  CHECK(s.window == 50);
  CHECK(s.tolerance == 0.05);
  CHECK(s.minMean == 0);
}

void TestWindowsMatch()
{
  double change = -1;
  // 4 % and 2 % changes: below 5 %.
  CHECK(SteadyStateDetector::WindowsMatch({100, 50}, {104, 49}, Settings(1, 0.05), change));
  CHECK(std::abs(change - 0.04) < 1e-12);
  // 5 % exactly is not below 5 %.
  CHECK(!SteadyStateDetector::WindowsMatch({100}, {105}, Settings(1, 0.05), change));
  // A species absent from both windows is skipped; one that appears is an infinite change.
  CHECK(SteadyStateDetector::WindowsMatch({100, 0}, {101, 0}, Settings(1, 0.05), change));
  CHECK(!SteadyStateDetector::WindowsMatch({100, 0}, {101, 1}, Settings(1, 0.05), change));
  CHECK(std::isinf(change));
  // minMean skips the rare species, both means <= minMean.
  CHECK(SteadyStateDetector::WindowsMatch({100, 0}, {101, 3}, Settings(1, 0.05, 3), change));
  CHECK(!SteadyStateDetector::WindowsMatch({100, 0}, {101, 4}, Settings(1, 0.05, 3), change));
  // Nothing compared: a match with a 0 change.
  CHECK(SteadyStateDetector::WindowsMatch({0}, {0}, Settings(1, 0.05), change));
  CHECK(change == 0);
  CHECK_THROWS(SteadyStateDetector::WindowsMatch({1}, {1, 2}, Settings(1, 0.05), change), "sizes differ");
}

void TestStopAfterTwoWindows()
{
  SteadyStateDetector detector(Settings(3, 0.05), 2);
  // Window 1: means 10 and 2. Window 2: means 20 and 2 (100 % change). Window 3:
  // means 20.5 and 2 (2.5 %): reached at sample 9.
  const double samples[9][2] = {{9, 2}, {10, 2}, {11, 2}, {19, 2}, {20, 2}, {21, 2}, {20, 2}, {20.5, 2}, {21, 2}};
  for (int k = 0; k < 9; ++k)
  {
    const bool reached = detector.AddSample({samples[k][0], samples[k][1]});
    CHECK(reached == (k == 8));
    CHECK(detector.Reached() == (k == 8));
    CHECK(detector.SampleCount() == static_cast<std::size_t>(k + 1));
  }
  const auto& changes = detector.WindowMaxRelativeChanges();
  CHECK(changes.size() == 3);
  CHECK(std::isnan(changes[0]));
  CHECK(std::abs(changes[1] - 1.0) < 1e-12);
  CHECK(std::abs(changes[2] - 0.025) < 1e-12);
  CHECK(detector.SteadyMeans().size() == 2);
  CHECK(std::abs(detector.SteadyMeans()[0] - 20.5) < 1e-12);
  CHECK(detector.SteadyMeans()[1] == 2);
  CHECK_THROWS(detector.AddSample({1, 1}), "after the steady state");
}

void TestEarliestStop()
{
  // A constant signal stops at the end of the second window, never earlier.
  SteadyStateDetector detector(Settings(2, 0.05), 1);
  CHECK(detector.SteadyMeans().empty());
  CHECK(!detector.AddSample({5}));
  CHECK(!detector.AddSample({5}));
  CHECK(detector.SteadyMeans().size() == 1);
  CHECK(!detector.AddSample({5}));
  CHECK(detector.AddSample({5}));
  CHECK(detector.Reached());
}

void TestGrowingSignalNeverStops()
{
  // A count that keeps growing by 10 % per window never reaches a 5 % steady state.
  SteadyStateDetector detector(Settings(1, 0.05), 1);
  double x = 100;
  for (int k = 0; k < 200; ++k, x *= 1.1) CHECK(!detector.AddSample({x}));
  CHECK(!detector.Reached());
}

void TestRejects()
{
  CHECK_THROWS(SteadyStateDetector(Settings(0, 0.05), 1), "window");
  CHECK_THROWS(SteadyStateDetector(Settings(1, 0), 1), "tolerance");
  CHECK_THROWS(SteadyStateDetector(Settings(1, NAN), 1), "tolerance");
  CHECK_THROWS(SteadyStateDetector(Settings(1, 0.05, -1), 1), "minMean");
  CHECK_THROWS(SteadyStateDetector(Settings(1, 0.05), 0), "no species");
  SteadyStateDetector detector(Settings(2, 0.05), 2);
  CHECK_THROWS(detector.AddSample({1}), "one count per species");
  CHECK_THROWS(detector.AddSample({1, -1}), "negative");
  CHECK_THROWS(detector.AddSample({1, INFINITY}), "not finite");
  CHECK(detector.SampleCount() == 0);
}

}  // namespace

int main()
{
  TestDefaults();
  TestWindowsMatch();
  TestStopAfterTwoWindows();
  TestEarliestStop();
  TestGrowingSignalNeverStops();
  TestRejects();
  return 0;
}
