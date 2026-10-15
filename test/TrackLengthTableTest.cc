/// \file TrackLengthTableTest.cc
/// \brief Plain-assert unit tests for TrackLengthTable (std only).

#include "scoring/TrackLengthTable.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
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
  std::ostringstream s1, s2;
  m1.WriteCsv(s1);
  m2.WriteCsv(s2);
  assert(s1.str() == s2.str());
}

static void TestSortedOutput()
{
  TrackLengthTable t;
  t.Add(MakeRow(1, 0));
  t.Add(MakeRow(0, 5));
  t.Add(MakeRow(0, 1));

  std::ostringstream csv;
  t.WriteCsv(csv);
  std::string s = csv.str();
  assert(s.rfind("run,event,primaryLength_nm", 0) == 0);
  const auto p01 = s.find("\n0,1,");
  const auto p05 = s.find("\n0,5,");
  const auto p10 = s.find("\n1,0,");
  assert(p01 != std::string::npos && p05 != std::string::npos && p10 != std::string::npos);
  assert(p01 < p05 && p05 < p10);

  std::ostringstream txt;
  t.WriteAscii(txt);
  s = txt.str();
  assert(s.rfind("# run event", 0) == 0);
  const auto q01 = s.find("\n0 1 ");
  const auto q05 = s.find("\n0 5 ");
  const auto q10 = s.find("\n1 0 ");
  assert(q01 != std::string::npos && q05 != std::string::npos && q10 != std::string::npos);
  assert(q01 < q05 && q05 < q10);
}

static void TestEndStrings()
{
  using E = TrackLengthTable::End;
  assert(std::string(TrackLengthTable::EndToString(E::Stopped)) == "stopped");
  assert(std::string(TrackLengthTable::EndToString(E::Escaped)) == "escaped");
  assert(std::string(TrackLengthTable::EndToString(E::Killed)) == "killed");
  TrackLengthTable t;
  t.Add(MakeRow(0, 0, 1., E::Killed));
  std::ostringstream csv;
  t.WriteCsv(csv);
  assert(csv.str().find(",killed,") != std::string::npos);
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
  TestSortedOutput();
  TestEndStrings();
  TestSummary();
  std::cout << "TrackLengthTableTest: all tests passed\n";
  return 0;
}
