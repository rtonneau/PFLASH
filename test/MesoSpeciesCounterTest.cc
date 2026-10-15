/// \file MesoSpeciesCounterTest.cc
/// \brief Plain-assert unit tests for MesoSpeciesCounter (no test framework,
/// no Geant4 runtime -- pure accumulation/merge/output logic only).

#include "scoring/MesoSpeciesCounter.hh"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

/// Count at (time, species), or -1 when absent (keeps failures assertions).
static long CountAt(const MesoSpeciesCounter& counter, double time, const std::string& species)
{
  const auto byTime = counter.GetCounts().find(time);
  if (byTime == counter.GetCounts().end()) return -1;
  const auto bySpecies = byTime->second.find(species);
  return bySpecies == byTime->second.end() ? -1 : bySpecies->second;
}

static void TestAddSumsSameTimeAndSpecies()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  counter.Add(5., "OH^0", 4);

  assert(CountAt(counter, 5., "OH^0") == 7);
}

static void TestAddKeepsTimesAndSpeciesSeparate()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  counter.Add(10., "OH^0", 2);
  counter.Add(5., "H_2^0", 1);

  assert(CountAt(counter, 5., "OH^0") == 3);
  assert(CountAt(counter, 10., "OH^0") == 2);
  assert(CountAt(counter, 5., "H_2^0") == 1);
}

static void TestMergeSumsPerTimeAndSpecies()
{
  MesoSpeciesCounter a;
  a.Add(5., "OH^0", 3);
  a.Add(10., "OH^0", 1);
  MesoSpeciesCounter b;
  b.Add(5., "OH^0", 4);
  b.Add(10., "e_aq^-1", 2);

  a.Merge(b);

  assert(CountAt(a, 5., "OH^0") == 7);
  assert(CountAt(a, 10., "OH^0") == 1);
  assert(CountAt(a, 10., "e_aq^-1") == 2);
}

static void TestMergeDoesNotModifyOther()
{
  MesoSpeciesCounter a;
  a.Add(5., "OH^0", 3);
  MesoSpeciesCounter b;
  b.Add(5., "OH^0", 4);

  a.Merge(b);

  assert(CountAt(b, 5., "OH^0") == 4);
  assert(b.GetCounts().size() == 1);
}

static void TestEmptyAndClear()
{
  MesoSpeciesCounter counter;
  assert(counter.Empty());

  counter.Add(5., "OH^0", 3);
  assert(!counter.Empty());

  counter.Clear();
  assert(counter.Empty());
  assert(counter.GetCounts().empty());
}

static void TestWriteCsvHeaderAndRowOrder()
{
  MesoSpeciesCounter counter;
  counter.Add(1000000., "OH^0", 2);
  counter.Add(5., "OH^0", 3);
  counter.Add(5., "H_2^0", 1);
  counter.Add(5., "OH^0", 4);

  std::ostringstream out;
  counter.WriteCsv(out);

  assert(out.str() ==
         "time_ns,species,count\n"
         "5,H_2^0,1\n"
         "5,OH^0,7\n"
         "1000000,OH^0,2\n");
}

static void TestWriteAsciiMeansOverTwoEvents()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  counter.Add(5., "OH^0", 4);
  counter.Add(5., "H_2^0", 2);
  counter.Add(1000000., "OH^0", 1);

  std::ostringstream out;
  counter.WriteAscii(out, 2);

  assert(out.str() ==
         "Time is in ns; mean count per event over 2 events\n"
         "5\n"
         "H_2^0 1\n"
         "OH^0 3.5\n"
         "1000000\n"
         "OH^0 0.5\n");
}

static void TestWriteAsciiZeroEventsGivesZeroMeans()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);

  std::ostringstream out;
  counter.WriteAscii(out, 0);

  assert(out.str().find("OH^0 0\n") != std::string::npos);
}

int main()
{
#ifdef _MSC_VER
  // Route Debug-CRT assert failures to stderr instead of a blocking dialog,
  // so a failing assert() exits the process instead of hanging CI/ctest.
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestAddSumsSameTimeAndSpecies();
  TestAddKeepsTimesAndSpeciesSeparate();
  TestMergeSumsPerTimeAndSpecies();
  TestMergeDoesNotModifyOther();
  TestEmptyAndClear();
  TestWriteCsvHeaderAndRowOrder();
  TestWriteAsciiMeansOverTwoEvents();
  TestWriteAsciiZeroEventsGivesZeroMeans();

  std::cout << "All MesoSpeciesCounter tests passed." << std::endl;
  return 0;
}
