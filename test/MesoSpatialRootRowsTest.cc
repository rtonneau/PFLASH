/// \file MesoSpatialRootRowsTest.cc
/// \brief Plain-assert tests for MesoSpatialRootRows (pure functions, no kernel).

#include "scoring/MesoSpatialRootRows.hh"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, 0);
  _CrtSetReportMode(_CRT_ERROR, 0);
#endif
  MesoSpatialFile::EventData ev;
  ev.runId = 2;
  ev.eventId = 5;
  MesoSpatialFile::Snapshot a;
  a.cellSize_nm = 10.;
  a.position_nm = {1., 2., 3., 4., 5., 6.};
  a.counts = {1, 0, 0, 2}; // cell0: s0=1 ; cell1: s1=2
  MesoSpatialFile::Snapshot b;
  b.cellSize_nm = 20.;
  b.position_nm = {7., 8., 9.};
  b.counts = {3, 4};
  ev.snapshots = {a, b};
  ev.records = {{1., 0}, {2., 0}, {3., 1}, {4., 9}}; // last: bad index, skipped

  OutputTable t = MesoSpatialRootRows::MakeTable();
  const std::size_t added = MesoSpatialRootRows::ExpandEvent(t, {"e_aq", "OH"}, ev);
  assert(added == 6);
  assert(t.Rows().size() == 6);
  assert(t.Columns().size() == 10);
  const auto &r0 = t.Rows()[0];
  assert(std::get<std::int64_t>(r0[0]) == 2 && std::get<std::int64_t>(r0[1]) == 5);
  assert(std::get<std::int64_t>(r0[2]) == 0 && std::get<double>(r0[3]) == 1.);
  assert(std::get<double>(r0[5]) == 1. && std::get<std::string>(r0[8]) == "e_aq");
  assert(std::get<std::int64_t>(r0[9]) == 1);
  const auto &r1 = t.Rows()[1];
  assert(std::get<std::string>(r1[8]) == "OH" && std::get<double>(r1[7]) == 6.);
  assert(std::get<std::int64_t>(r1[9]) == 2);
  const auto &r5 = t.Rows()[5]; // record 2, species OH of the single b cell
  assert(std::get<std::int64_t>(r5[2]) == 2 && std::get<double>(r5[4]) == 20.);
  assert(std::get<std::int64_t>(r5[9]) == 4);

  // Staging: append, count, take empties.
  MesoSpatialRootRows::AppendStaged(t);
  assert(MesoSpatialRootRows::StagedRowCount() == 6);
  OutputTable taken = MesoSpatialRootRows::TakeStaged();
  assert(taken.Rows().size() == 6);
  assert(MesoSpatialRootRows::StagedRowCount() == 0);

  std::cout << "MesoSpatialRootRowsTest passed\n";
  return 0;
}
