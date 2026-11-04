/// \file PhysicsInteractionCounterTest.cc
/// Dependencies: Standard-library-only source and portable project code; current CMake target links
/// Geant4 without initializing its kernel. Optional MSVC CRT diagnostics.
/// Thread safety: Sequential test process; no concurrent execution within this test.
/// \brief Plain-assert unit tests for PhysicsInteractionCounter (no test
/// framework, no Geant4 runtime -- exercises per-event folding, zero-fill,
/// merge and ToTable only).

#include "scoring/PhysicsInteractionCounter.hh"

#include <cassert>
#include <cmath>
#include <iostream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static void TestCommitFoldsPerEvent()
{
  PhysicsInteractionCounter c;
  c.Record("A");
  c.Record("A");
  c.Record("B");
  c.CommitEvent();
  c.Record("A");
  c.CommitEvent();

  assert(c.CommittedEvents() == 2);
  assert(c.GetStats().at("A").Sum() == 3.0);
  assert(c.GetStats().at("A").SumSq() == 5.0);  // 2^2 + 1^2
  assert(c.GetStats().at("B").Sum() == 1.0);
}

static void TestUncommittedEventNotCounted()
{
  PhysicsInteractionCounter c;
  c.Record("A");
  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 0);
  c.DiscardEvent();
  c.CommitEvent();
  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 1);
}

static void TestZeroFill()
{
  PhysicsInteractionCounter c;
  c.Record("A");
  c.CommitEvent();
  c.CommitEvent();  // no A
  c.Record("A");
  c.Record("A");
  c.Record("A");
  c.CommitEvent();
  c.CommitEvent();  // no A

  const EventStat& a = c.GetStats().at("A");
  assert(c.CommittedEvents() == 4);
  assert(a.Mean(c.CommittedEvents()) == 1.0);  // (1+0+3+0)/4
  assert(a.SumSq() == 10.0);                   // 1 + 9, zeros add 0
}

static void TestMergeEqualsSingleFeed()
{
  PhysicsInteractionCounter single, a, b;
  for (int i = 0; i < 3; ++i) {
    PhysicsInteractionCounter& target = (i % 2 == 0) ? a : b;
    for (int k = 0; k <= i; ++k) {
      single.Record("A");
      target.Record("A");
    }
    if (i == 1) {
      single.Record("B");
      target.Record("B");
    }
    single.CommitEvent();
    target.CommitEvent();
  }
  a.Merge(b);

  assert(a.CommittedEvents() == single.CommittedEvents());
  for (const char* label : {"A", "B"}) {
    assert(a.GetStats().at(label).Sum() == single.GetStats().at(label).Sum());
    assert(a.GetStats().at(label).SumSq() == single.GetStats().at(label).SumSq());
  }
}

static void TestMergeDoesNotModifyOther()
{
  PhysicsInteractionCounter a, b;
  b.Record("A");
  b.CommitEvent();
  a.Merge(b);
  assert(b.CommittedEvents() == 1);
  assert(b.GetStats().at("A").Sum() == 1.0);
}

static void TestClearResetsEverything()
{
  PhysicsInteractionCounter c;
  c.Record("A");
  c.CommitEvent();
  c.Record("A");
  c.Clear();
  c.CommitEvent();
  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 1);
}

static void TestToTable()
{
  PhysicsInteractionCounter c;
  c.Record("B");
  c.CommitEvent();
  c.Record("A");
  c.Record("A");
  c.CommitEvent();

  const OutputTable t = c.ToTable();
  assert(t.Name() == "physicsInteractions");
  const char* names[] = {"process", "nEvents", "countSum", "countSumSq", "countMean", "countStdErr"};
  assert(t.Columns().size() == 6);
  for (size_t i = 0; i < 6; ++i) assert(t.Columns()[i].name == names[i]);
  assert(t.Rows().size() == 2);
  const auto& row = t.Rows()[0];  // sorted: A first
  assert(std::get<std::string>(row[0]) == "A");
  assert(std::get<std::int64_t>(row[1]) == 2);
  assert(std::get<double>(row[2]) == 2.0);
  assert(std::get<double>(row[3]) == 4.0);
  assert(std::get<double>(row[4]) == 1.0);
  // values 2,0: sample variance (4 - 4/2)/1 = 2; stderr = sqrt(2/2) = 1
  assert(std::abs(std::get<double>(row[5]) - 1.0) < 1e-12);
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

  TestCommitFoldsPerEvent();
  TestUncommittedEventNotCounted();
  TestZeroFill();
  TestMergeEqualsSingleFeed();
  TestMergeDoesNotModifyOther();
  TestClearResetsEverything();
  TestToTable();

  std::cout << "All PhysicsInteractionCounter tests passed." << std::endl;
  return 0;
}
