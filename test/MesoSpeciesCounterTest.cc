/// \file MesoSpeciesCounterTest.cc
/// Dependencies: Standard library and portable project code; no Geant4 dependency. Optional MSVC
/// CRT diagnostics.
/// Thread safety: Sequential test process; no concurrent execution within this test.
/// \brief Plain-assert unit tests for MesoSpeciesCounter (no test framework,
/// no Geant4 runtime -- pure accumulation/merge/output logic only).

#include "scoring/MesoSpeciesCounter.hh"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static bool Near(double a, double b)
{
  return std::fabs(a - b) <= 1e-9 * (1. + std::fabs(a) + std::fabs(b));
}

static const MesoSpeciesCounter::Stat& StatAt(const MesoSpeciesCounter& c, double time,
                                              const std::string& species)
{
  return c.GetStats().at(time).at(species);
}

static void TestAddIsUncommittedUntilCommit()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  assert(counter.Empty());
  assert(counter.CommittedEvents() == 0);
  counter.CommitEvent(100.);
  assert(!counter.Empty());
  assert(counter.CommittedEvents() == 1);
}

static void TestCommitFoldsCountsAndG()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  counter.Add(5., "OH^0", 4);  // same (time, species) sums within the event
  counter.Add(10., "OH^0", 2);
  counter.CommitEvent(200.);

  assert(Near(StatAt(counter, 5., "OH^0").count.Sum(), 7.));
  assert(Near(StatAt(counter, 5., "OH^0").count.SumSq(), 49.));
  assert(Near(StatAt(counter, 5., "OH^0").g.Sum(), 7. / 200. * 100.));
  assert(Near(StatAt(counter, 10., "OH^0").g.Sum(), 1.));
}

static void TestZeroFillAcrossEvents()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 4);
  counter.CommitEvent(100.);
  counter.Add(5., "H_2^0", 2);  // OH^0 absent in event 2
  counter.CommitEvent(100.);

  const auto& oh = StatAt(counter, 5., "OH^0");
  assert(counter.CommittedEvents() == 2);
  assert(Near(oh.count.Sum(), 4.));
  assert(Near(oh.count.Mean(counter.CommittedEvents()), 2.));
  assert(Near(StatAt(counter, 5., "H_2^0").count.Mean(2), 1.));
  assert(Near(oh.g.Mean(2), 2.));
}

static void TestZeroEdepEventAddsCountsButNoG()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 4);
  counter.CommitEvent(0.);

  assert(Near(StatAt(counter, 5., "OH^0").count.Sum(), 4.));
  assert(StatAt(counter, 5., "OH^0").g.Sum() == 0.);
}

static void TestDiscardDropsCurrentEvent()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 4);
  counter.DiscardEvent();
  counter.CommitEvent(100.);
  assert(counter.Empty());
  assert(counter.CommittedEvents() == 1);
}

static void TestMergeEqualsSingleFeed()
{
  MesoSpeciesCounter a, b, single;
  a.Add(5., "OH^0", 3);
  a.CommitEvent(100.);
  b.Add(5., "OH^0", 5);
  b.Add(10., "e_aq^-1", 2);
  b.CommitEvent(50.);
  b.Add(10., "e_aq^-1", 1);
  b.CommitEvent(25.);

  single.Add(5., "OH^0", 3);
  single.CommitEvent(100.);
  single.Add(5., "OH^0", 5);
  single.Add(10., "e_aq^-1", 2);
  single.CommitEvent(50.);
  single.Add(10., "e_aq^-1", 1);
  single.CommitEvent(25.);

  a.Merge(b);

  assert(a.CommittedEvents() == single.CommittedEvents());
  const OutputTable ta = a.ToTable();
  const OutputTable ts = single.ToTable();
  assert(ta.Rows().size() == ts.Rows().size());
  for (std::size_t i = 0; i < ta.Rows().size(); ++i) {
    assert(ta.Rows()[i] == ts.Rows()[i]);
  }
}

static void TestMergeDoesNotModifyOther()
{
  MesoSpeciesCounter a, b;
  a.Add(5., "OH^0", 3);
  a.CommitEvent(100.);
  b.Add(5., "OH^0", 4);
  b.CommitEvent(100.);
  a.Merge(b);
  assert(Near(StatAt(b, 5., "OH^0").count.Sum(), 4.));
  assert(b.CommittedEvents() == 1);
}

static void TestClear()
{
  MesoSpeciesCounter counter;
  counter.Add(5., "OH^0", 3);
  counter.CommitEvent(100.);
  counter.Add(5., "OH^0", 3);
  counter.Clear();
  assert(counter.Empty());
  assert(counter.CommittedEvents() == 0);
  counter.CommitEvent(100.);
  assert(counter.Empty());
}

static void TestToTableColumnsAndRows()
{
  MesoSpeciesCounter counter;
  counter.Add(1000000., "OH^0", 2);
  counter.Add(5., "OH^0", 3);
  counter.Add(5., "H_2^0", 1);
  counter.CommitEvent(100.);

  const OutputTable table = counter.ToTable();
  const OutputTable expected = SpeciesSchema::MakeTable("speciesMeso", "");
  assert(table.Name() == "speciesMeso");
  assert(table.Columns().size() == expected.Columns().size());
  for (std::size_t i = 0; i < table.Columns().size(); ++i) {
    assert(table.Columns()[i].name == expected.Columns()[i].name);
  }
  assert(table.Columns()[0].name == "time_ns");
  assert(table.Columns()[1].name == "species");
  assert(table.Columns()[2].name == "nEvents");

  assert(table.Rows().size() == 3);
  // time ascending, then species sorted
  assert(std::get<double>(table.Rows()[0][0]) == 5.);
  assert(std::get<std::string>(table.Rows()[0][1]) == "H_2^0");
  assert(std::get<std::string>(table.Rows()[1][1]) == "OH^0");
  assert(std::get<double>(table.Rows()[2][0]) == 1000000.);
  assert(std::get<std::int64_t>(table.Rows()[1][2]) == 1);
  assert(Near(std::get<double>(table.Rows()[1][3]), 3.));
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

  TestAddIsUncommittedUntilCommit();
  TestCommitFoldsCountsAndG();
  TestZeroFillAcrossEvents();
  TestZeroEdepEventAddsCountsButNoG();
  TestDiscardDropsCurrentEvent();
  TestMergeEqualsSingleFeed();
  TestMergeDoesNotModifyOther();
  TestClear();
  TestToTableColumnsAndRows();

  std::cout << "All MesoSpeciesCounter tests passed." << std::endl;
  return 0;
}
