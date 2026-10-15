/// \file MesoSettingsTest.cc
/// \brief Plain-assert unit tests for MesoSettings (no Geant4 runtime):
/// pixel count, log time grid, defaults and the /chem/meso/enable switch.
#include "chemistry/MesoSettings.hh"

#include <cassert>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
template <class F>
bool Throws(F f)
{
  try {
    f();
  }
  catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

void TestPixelCount()
{
  assert(MesoSettings::PixelCount(3.2, 0.00625) == 512);
  assert(MesoSettings::PixelCount(1.0, 1.0) == 1);
  assert(MesoSettings::PixelCount(1000.0, 0.00625) == 65536);
  assert(MesoSettings::PixelCountCapped(1000.0, 0.00625));
  assert(MesoSettings::PixelCount(400.0, 0.00625) == 65536);
  assert(!MesoSettings::PixelCountCapped(400.0, 0.00625));
  assert(MesoSettings::PixelCount(1000.0, 0.00625, 1024) == 1024);
  // Exact tie (|4 - 3| == |2 - 3|): the larger p wins.
  assert(MesoSettings::PixelCount(4.0, 3.0) == 2);
}

void TestLogTimeGrid()
{
  const auto grid = MesoSettings::LogTimeGrid(5., 50., 10);
  assert(grid.size() == 11);
  assert(grid.front() == 5.);
  assert(grid.back() == 50.);
  for (std::size_t i = 1; i < grid.size(); ++i) assert(grid[i] > grid[i - 1]);

  const auto shortGrid = MesoSettings::LogTimeGrid(5., 7., 10);
  assert(shortGrid.back() == 7.);
  assert(shortGrid.front() == 5.);
  assert(shortGrid.size() == 3);  // 5, 5*10^0.1 = 6.29, 7
}

void TestInvalid()
{
  assert(Throws([] { MesoSettings::LogTimeGrid(5., 5., 10); }));
  assert(Throws([] { MesoSettings::LogTimeGrid(6., 5., 10); }));
  assert(Throws([] { MesoSettings::LogTimeGrid(5., 50., 0); }));
  assert(Throws([] { MesoSettings::PixelCount(0., 1.); }));
  assert(Throws([] { MesoSettings::PixelCount(1., 0.); }));
}

void TestDefaults()
{
  const auto& settings = MesoSettings::Current();
  assert(settings.spatialOutput == false);
  assert(settings.enabled == true);
  assert(settings.enabledExplicitly == false);
}

void TestStageEnabled()
{
  MesoSettings::Values values;  // default: enabled, not set explicitly
  assert(MesoSettings::StageEnabled(values, false));
  // SBS has no hand-over: the default (true) silently means off.
  assert(!MesoSettings::StageEnabled(values, true));
  assert(!MesoSettings::ExplicitlyEnabledWithSbs(values, true));
  assert(!MesoSettings::ExplicitlyEnabledWithSbs(values, false));

  values.enabled = false;
  values.enabledExplicitly = true;  // /chem/meso/enable false
  assert(!MesoSettings::StageEnabled(values, false));
  assert(!MesoSettings::StageEnabled(values, true));
  assert(!MesoSettings::ExplicitlyEnabledWithSbs(values, true));

  values.enabled = true;  // /chem/meso/enable true
  assert(MesoSettings::StageEnabled(values, false));
  assert(!MesoSettings::StageEnabled(values, true));
  assert(MesoSettings::ExplicitlyEnabledWithSbs(values, true));
  assert(!MesoSettings::ExplicitlyEnabledWithSbs(values, false));
}
}  // namespace

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestPixelCount();
  TestLogTimeGrid();
  TestInvalid();
  TestDefaults();
  TestStageEnabled();
  std::cout << "MesoSettingsTest: all tests passed\n";
  return 0;
}
