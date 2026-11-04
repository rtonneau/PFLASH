/// \file TrackLengthTableTest.cc
/// Dependencies: Standard library and portable project code; no Geant4 dependency. Optional MSVC
/// CRT diagnostics.
/// Thread safety: Sequential test process; no concurrent execution within this test.
/// \brief Plain-assert unit tests for TrackLengthTable (std only).

#include "scoring/TrackLengthTable.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#include <cstdint>
#include <variant>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static TrackLengthTable::Row MakeRow(int run, int event, double len = 1.,
                                     TrackLengthTable::End end = TrackLengthTable::End::Stopped)
{
  TrackLengthTable::Row r;
  r.run = run;
  r.event = event;
  r.primaryLength_nm = len;
  r.primaryEnd = end;
  return r;
}

static bool Near(double a, double b)
{
  return std::fabs(a - b) < 1e-9;
}

static void TestAddSizeEmptyClear()
{
  TrackLengthTable t;
  assert(t.Empty() && t.Size() == 0);
  t.Add(MakeRow(0, 0));
  t.Add(MakeRow(0, 1));
  assert(!t.Empty() && t.Size() == 2);
  t.Clear();
  assert(t.Empty() && t.Size() == 0);
}

static void TestMergeOrderIndependent()
{
  TrackLengthTable a, b, m1, m2;
  a.Add(MakeRow(0, 2));
  a.Add(MakeRow(1, 0));
  b.Add(MakeRow(0, 0));
  b.Add(MakeRow(0, 1));
  m1.Merge(a);
  m1.Merge(b);
  m2.Merge(b);
  m2.Merge(a);
  assert(m1.Size() == 4 && m2.Size() == 4);
  assert(a.Size() == 2 && b.Size() == 2);
  assert(m1.ToTable().Rows() == m2.ToTable().Rows());
}

static void TestToTable()
{
  using E = TrackLengthTable::End;
  TrackLengthTable t;
  TrackLengthTable::Row r = MakeRow(1, 0, 3., E::Killed);
  r.primaryEkin0_keV = 10.;
  r.primaryEkinEnd_keV = 2.;
  r.secondaryFirstGen_nm = 4.;
  r.nSecondaryFirstGen = 5;
  r.secondaryAll_nm = 6.;
  r.nSecondaryAll = 7;
  t.Add(r);
  t.Add(MakeRow(0, 5));
  t.Add(MakeRow(0, 1));

  const OutputTable table = t.ToTable();
  assert(table.Name() == "trackLengths");
  const char* names[] = {"run", "event", "primaryLength_nm", "primaryEkinStart_keV", "primaryEkinEnd_keV",
                         "primaryEnd", "secondaryFirstGenLength_nm", "nSecondaryFirstGen",
                         "secondaryAllLength_nm", "nSecondaryAll"};
  assert(table.Columns().size() == 10);
  for (std::size_t i = 0; i < 10; ++i) assert(table.Columns()[i].name == names[i]);
  assert(table.Columns()[5].type == OutputTable::ColumnType::String);
  assert(table.Columns()[0].type == OutputTable::ColumnType::Int64);
  assert(table.Columns()[2].unit == "nm" && table.Columns()[3].unit == "keV");

  // sorted by (run, event)
  assert(table.Rows().size() == 3);
  assert(std::get<std::int64_t>(table.Rows()[0][0]) == 0 && std::get<std::int64_t>(table.Rows()[0][1]) == 1);
  assert(std::get<std::int64_t>(table.Rows()[1][1]) == 5);
  const auto& last = table.Rows()[2];
  assert(std::get<std::int64_t>(last[0]) == 1);
  assert(Near(std::get<double>(last[2]), 3.));
  assert(Near(std::get<double>(last[3]), 10.));
  assert(Near(std::get<double>(last[4]), 2.));
  assert(std::get<std::string>(last[5]) == "killed");
  assert(Near(std::get<double>(last[6]), 4.));
  assert(std::get<std::int64_t>(last[7]) == 5);
  assert(Near(std::get<double>(last[8]), 6.));
  assert(std::get<std::int64_t>(last[9]) == 7);
  assert(std::get<std::string>(table.Rows()[0][5]) == "stopped");
}

static void TestEndStrings()
{
  using E = TrackLengthTable::End;
  assert(std::string(TrackLengthTable::EndToString(E::Stopped)) == "stopped");
  assert(std::string(TrackLengthTable::EndToString(E::Escaped)) == "escaped");
  assert(std::string(TrackLengthTable::EndToString(E::Killed)) == "killed");
}

static void TestSummary()
{
  using E = TrackLengthTable::End;
  TrackLengthTable empty;
  auto s0 = empty.Summarize();
  assert(s0.nEvents == 0 && s0.primaryLength_nm.mean == 0. && s0.primaryLength_nm.sem == 0.);

  TrackLengthTable one;
  one.Add(MakeRow(0, 0, 5.));
  auto s1 = one.Summarize();
  assert(s1.nEvents == 1 && Near(s1.primaryLength_nm.mean, 5.) && s1.primaryLength_nm.sem == 0.);

  TrackLengthTable t;
  t.Add(MakeRow(0, 0, 2., E::Stopped));
  t.Add(MakeRow(0, 1, 4., E::Escaped));
  t.Add(MakeRow(0, 2, 6., E::Killed));
  t.Add(MakeRow(0, 3, 8., E::Stopped));
  auto s = t.Summarize();
  // mean 5, sample variance 20/3, SEM = sqrt(20/3 / 4)
  assert(s.nEvents == 4);
  assert(Near(s.primaryLength_nm.mean, 5.));
  assert(Near(s.primaryLength_nm.sem, std::sqrt(20. / 3. / 4.)));
  assert(s.nStopped == 2 && s.nEscaped == 1 && s.nKilled == 1);
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestAddSizeEmptyClear();
  TestMergeOrderIndependent();
  TestToTable();
  TestEndStrings();
  TestSummary();
  std::cout << "TrackLengthTableTest: all tests passed\n";
  return 0;
}
