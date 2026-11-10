#include "HistoryConfig.hh"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>

#include "TestCheck.hh"

using namespace ptirt;

namespace
{

HistoryConfig Read(const std::string& text)
{
  std::istringstream in(text);
  return ReadHistoryConfig(in, "base");
}

const std::string kDeterministic =
    "# a comment\n"
    "mode = deterministic\n"
    "\n"
    "libraryDir = lib\n"
    "  scheduleFile=schedule.txt  \n"
    "endpointNs = 3e6\n"
    "chemistrySeed = 7\n";

const std::string kPoisson =
    "mode = poisson\n"
    "libraryDir = /abs/lib\n"
    "endpointNs = 3e6\n"
    "chemistrySeed = 2147483647\n"
    "rateHz = 2000\n"
    "exposureS = 2e-3\n"
    "sourceSeed = 11\n";

void TestDeterministic()
{
  const auto config = Read(kDeterministic);
  CHECK(config.mode == HistoryMode::Deterministic);
  CHECK(config.libraryDir == std::filesystem::path("base/lib").lexically_normal());
  CHECK(config.scheduleFile == std::filesystem::path("base/schedule.txt").lexically_normal());
  CHECK(config.endpointNs == 3e6);
  CHECK(config.chemistrySeed == 7);
  // Defaults.
  CHECK(config.worldHalfSideNm == 1.0e6);
  CHECK(config.halfBoxNm == 0);
  CHECK(config.gridMinAgeNs == 1);
  CHECK(config.gridPointsPerDecade == 5);
  CHECK(!config.newOldContactLimit.has_value());
  CHECK(std::string(HistoryModeName(config.mode)) == "deterministic");

  // The contact limit: any integer >= 0, in every mode; 0 is a limit, not "off".
  CHECK(Read(kDeterministic + "newOldContactLimit = 0\n").newOldContactLimit == std::optional<std::uint64_t>(0));
  CHECK(Read(kDeterministic + "newOldContactLimit = 12345\n").newOldContactLimit ==
        std::optional<std::uint64_t>(12345));
  CHECK(Read(kPoisson + "newOldContactLimit = 7\n").newOldContactLimit == std::optional<std::uint64_t>(7));

  const auto boxed = Read(kDeterministic + "worldHalfSideUm = 3\nhalfBoxUm = 1.5\ngridMinAgeNs = 0.1\n"
                                           "gridPointsPerDecade = 10\n");
  CHECK(boxed.worldHalfSideNm == 3000);
  CHECK(boxed.halfBoxNm == 1500);
  CHECK(boxed.gridMinAgeNs == 0.1);
  CHECK(boxed.gridPointsPerDecade == 10);

  const auto single = Read("mode = single\nlibraryDir = lib\nscheduleFile = s.txt\nendpointNs = 10\n"
                           "chemistrySeed = 1\nsingleSource = 2\n");
  CHECK(single.mode == HistoryMode::Single);
  CHECK(single.singleSource == 2);
}

void TestPoisson()
{
  const auto config = Read(kPoisson);
  CHECK(config.mode == HistoryMode::Poisson);
  CHECK(config.libraryDir == std::filesystem::path("/abs/lib").lexically_normal());
  CHECK(config.chemistrySeed == 2147483647);
  CHECK(config.poisson.rateHz == 2000);
  CHECK(config.poisson.exposureS == 2e-3);
  CHECK(config.poisson.sourceSeed == 11);
  CHECK(config.poisson.regionCenterNm[0] == 0 && config.poisson.regionCenterNm[2] == 0);
  CHECK(config.poisson.regionHalfSizeNm[1] == 50000);

  const auto region = Read(kPoisson + "regionCenterUm = 1 -2 3\nregionHalfSizeUm = 20\n");
  CHECK(region.poisson.regionCenterNm[0] == 1000 && region.poisson.regionCenterNm[1] == -2000 &&
        region.poisson.regionCenterNm[2] == 3000);
  CHECK(region.poisson.regionHalfSizeNm[0] == 20000 && region.poisson.regionHalfSizeNm[2] == 20000);
  const auto anisotropic = Read(kPoisson + "regionHalfSizeUm = 1 0 3\n");
  CHECK(anisotropic.poisson.regionHalfSizeNm[1] == 0 && anisotropic.poisson.regionHalfSizeNm[2] == 3000);
}

void TestRejections()
{
  // Syntax.
  CHECK_THROWS(Read(kDeterministic + "this line has no equals sign\n"), "line 8: expected 'key = value'");
  CHECK_THROWS(Read(kDeterministic + " = 3\n"), "missing key");
  CHECK_THROWS(Read(kDeterministic + "colour = blue\n"), "unknown key 'colour'");
  CHECK_THROWS(Read(kDeterministic + "endpointNs = \n"), "empty value");
  CHECK_THROWS(Read(kDeterministic + "chemistrySeed = 8\n"), "repeated (first on line 7)");
  // Mode.
  CHECK_THROWS(Read("libraryDir = lib\n"), "missing required key 'mode'");
  CHECK_THROWS(Read("mode = burst\n"), "not one of deterministic, single, poisson");
  CHECK_THROWS(Read(kDeterministic + "rateHz = 5\n"), "not used by mode deterministic");
  CHECK_THROWS(Read(kDeterministic + "singleSource = 0\n"), "not used by mode deterministic");
  CHECK_THROWS(Read(kPoisson + "scheduleFile = s.txt\n"), "not used by mode poisson");
  // Required keys.
  CHECK_THROWS(Read("mode = deterministic\nlibraryDir = lib\nendpointNs = 10\nchemistrySeed = 1\n"),
               "missing required key 'scheduleFile'");
  CHECK_THROWS(Read("mode = deterministic\nscheduleFile = s\nendpointNs = 10\nchemistrySeed = 1\n"),
               "missing required key 'libraryDir'");
  CHECK_THROWS(Read("mode = single\nlibraryDir = lib\nscheduleFile = s\nendpointNs = 10\nchemistrySeed = 1\n"),
               "missing required key 'singleSource'");
  CHECK_THROWS(Read("mode = poisson\nlibraryDir = lib\nendpointNs = 3e6\nchemistrySeed = 1\nrateHz = 1\n"
                    "exposureS = 1e-3\n"),
               "missing required key 'sourceSeed'");
  // Numbers.
  CHECK_THROWS(Read("mode = deterministic\nlibraryDir = l\nscheduleFile = s\nendpointNs = abc\nchemistrySeed = 1\n"),
               "line 4: endpointNs: 'abc' is not a finite number");
  CHECK_THROWS(Read("mode = deterministic\nlibraryDir = l\nscheduleFile = s\nendpointNs = inf\nchemistrySeed = 1\n"),
               "not a finite number");
  CHECK_THROWS(Read("mode = deterministic\nlibraryDir = l\nscheduleFile = s\nendpointNs = -1\nchemistrySeed = 1\n"),
               "must be positive");
  CHECK_THROWS(Read("mode = deterministic\nlibraryDir = l\nscheduleFile = s\nendpointNs = 1 2\nchemistrySeed = 1\n"),
               "expected one number");
  // Seeds: 1..2147483647, plain digits.
  for (const auto* seed : {"0", "-1", "+3", "2147483648", "99999999999999999999", "1.5", "7x"})
    CHECK_THROWS(Read("mode = deterministic\nlibraryDir = l\nscheduleFile = s\nendpointNs = 10\nchemistrySeed = " +
                      std::string(seed) + "\n"),
                 "chemistrySeed");
  CHECK_THROWS(Read(kDeterministic + "gridPointsPerDecade = 0\n"), "gridPointsPerDecade");
  CHECK_THROWS(Read(kDeterministic + "gridPointsPerDecade = 101\n"), "gridPointsPerDecade");
  CHECK_THROWS(Read(kDeterministic + "gridMinAgeNs = 0.001\n"), "at least 0.01 ns");
  CHECK_THROWS(Read(kDeterministic + "gridMinAgeNs = 3e6\n"), "smaller than endpointNs");
  // Contact limit: a non-negative integer, plain digits.
  for (const auto* limit : {"-1", "+3", "1.5", "1e3", "abc", "0x10", "99999999999999999999"})
    CHECK_THROWS(Read(kDeterministic + "newOldContactLimit = " + std::string(limit) + "\n"), "newOldContactLimit");
  CHECK_THROWS(Read(kDeterministic + "newOldContactLimit = \n"), "empty value");
  CHECK_THROWS(Read(kDeterministic + "newOldContactLimit = 1\nnewOldContactLimit = 2\n"), "repeated");
  // Geometry.
  CHECK_THROWS(Read(kDeterministic + "worldHalfSideUm = 0\n"), "worldHalfSideUm: must be positive");
  CHECK_THROWS(Read(kDeterministic + "halfBoxUm = -1\n"), "zero (no box) or positive");
  CHECK_THROWS(Read(kDeterministic + "halfBoxUm = 1000\n"), "smaller than the world");
  // Poisson parameters.
  CHECK_THROWS(Read(kPoisson + "regionCenterUm = 1 2\n"), "three numbers");
  CHECK_THROWS(Read(kPoisson + "regionHalfSizeUm = 1 2\n"), "one number or three");
  CHECK_THROWS(Read(kPoisson + "regionHalfSizeUm = -1\n"), "must not be negative");
  CHECK_THROWS(Read("mode = poisson\nlibraryDir = l\nendpointNs = 2e6\nchemistrySeed = 1\nrateHz = 1\n"
                    "exposureS = 2e-3\nsourceSeed = 1\n"),
               "later than the exposure window");
  CHECK_THROWS(Read("mode = poisson\nlibraryDir = l\nendpointNs = 3e6\nchemistrySeed = 1\nrateHz = 0\n"
                    "exposureS = 2e-3\nsourceSeed = 1\n"),
               "rateHz: must be positive");
  // Missing file.
  CHECK_THROWS(ReadHistoryConfigFile("does-not-exist/none.cfg"), "cannot open");
}

void TestRecordGrid()
{
  // No arrival but the one at 0: log points from 1 ns, 1 per decade, then the endpoint.
  const auto simple = BuildRecordGrid({0.0}, 1000.0, 1.0, 1);
  CHECK(simple.size() == 4);
  CHECK(simple[0] == 1.0 && simple[1] == 10.0 && simple[2] == 100.0 && simple[3] == 1000.0);

  // A later arrival restarts the log ladder; points near it are dropped.
  const auto grid = BuildRecordGrid({0.0, 50.0}, 200.0, 1.0, 2);
  for (std::size_t i = 1; i < grid.size(); ++i) CHECK(grid[i] > grid[i - 1]);
  CHECK(grid.back() == 200.0);
  bool sawRestart = false;
  for (const double t : grid)
  {
    CHECK(std::abs(t - 50.0) >= kGridGuardNs);
    if (t == 51.0) sawRestart = true;
  }
  CHECK(sawRestart);

  // A point from the 0 anchor that falls just after an arrival is dropped:
  // 0 + 1 * 10^(2/1) = 100 ns lies on the arrival at 100.002 ns.
  const auto guarded = BuildRecordGrid({100.002}, 1000.0, 1.0, 1);
  for (const double t : guarded) CHECK(std::abs(t - 100.002) >= kGridGuardNs);
  CHECK(guarded.front() == 1.0);

  // No arrival at all (zero Poisson arrivals): the 0 anchor alone.
  CHECK(BuildRecordGrid({}, 1000.0, 1.0, 1).size() == 4);

  CHECK_THROWS(BuildRecordGrid({0.0}, 0.0, 1.0, 1), "endpoint");
  CHECK_THROWS(BuildRecordGrid({0.0}, 10.0, 0.0, 1), "minimum age");
  CHECK_THROWS(BuildRecordGrid({0.0}, 10.0, 1.0, 0), "points per decade");
  CHECK_THROWS(BuildRecordGrid({10.0}, 10.0, 1.0, 1), "arrival time");
  CHECK_THROWS(BuildRecordGrid({-1.0}, 10.0, 1.0, 1), "arrival time");
}

}  // namespace

int main()
{
  TestDeterministic();
  TestPoisson();
  TestRejections();
  TestRecordGrid();
  std::puts("HistoryConfigTest passed");
  return 0;
}
