/// \file ReactionCounterTest.cc
/// Dependencies: Geant4 types/libraries and project code; no kernel initialization. Optional MSVC
/// CRT diagnostics.
/// Thread safety: Sequential test process; no concurrent execution within this test.
/// \brief Plain-assert unit tests for ReactionCounter (no test framework, no
/// Geant4 runtime -- exercises per-event folding, zero-fill, merge, binning
/// and ToTables only).

#include "scoring/ReactionCounter.hh"

#include "G4SystemOfUnits.hh"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static const G4String kHH = "H + H -> H2";
static const G4String kEE = "e_aq + e_aq -> H2 + OHm + OHm";

// --- BinFor --------------------------------------------------------------

static void TestBinForExactEdgeReturnsThatEdge()
{
  assert(ReactionCounter::BinFor(10 * CLHEP::picosecond) == 10 * CLHEP::picosecond);
}

static void TestBinForBetweenEdgesReturnsNextEdge()
{
  // Between the 1ps and 10ps edges -> falls into the 10ps bin.
  assert(ReactionCounter::BinFor(5 * CLHEP::picosecond) == 10 * CLHEP::picosecond);
}

static void TestBinForBelowFirstEdgeReturnsFirstEdge()
{
  assert(ReactionCounter::BinFor(0.1 * CLHEP::picosecond) == 1 * CLHEP::picosecond);
}

static void TestBinForAboveLastEdgeClampsToLastEdge()
{
  assert(ReactionCounter::BinFor(1 * CLHEP::second) == 999999 * CLHEP::picosecond);
}

// --- Record / CommitEvent --------------------------------------------------

static void TestCommitFoldsPerEvent()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kEE, 10 * CLHEP::picosecond);
  c.CommitEvent();
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.CommitEvent();

  assert(c.CommittedEvents() == 2);
  const auto& stats = c.GetStats();
  assert(stats.at(1 * CLHEP::picosecond).at(kHH).Sum() == 3.0);
  assert(stats.at(1 * CLHEP::picosecond).at(kHH).SumSq() == 5.0);  // 2^2 + 1^2
  assert(stats.at(10 * CLHEP::picosecond).at(kEE).Sum() == 1.0);
}

static void TestRecordDifferentBinsStaySeparate()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kHH, 5 * CLHEP::picosecond);  // falls in the 10 ps bin
  c.CommitEvent();

  assert(c.GetStats().at(1 * CLHEP::picosecond).at(kHH).Sum() == 1.0);
  assert(c.GetStats().at(10 * CLHEP::picosecond).at(kHH).Sum() == 1.0);
}

static void TestUncommittedEventNotCounted()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 0);
  c.DiscardEvent();
  c.CommitEvent();
  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 1);
}

static void TestZeroFill()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.CommitEvent();
  c.CommitEvent();  // no firing
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.CommitEvent();
  c.CommitEvent();  // no firing

  const EventStat& s = c.GetStats().at(1 * CLHEP::picosecond).at(kHH);
  assert(c.CommittedEvents() == 4);
  assert(s.Mean(c.CommittedEvents()) == 1.0);  // (1+0+3+0)/4
  assert(s.SumSq() == 10.0);                   // 1 + 9, zeros add 0
}

// --- Merge -----------------------------------------------------------------

static void FeedEvent(ReactionCounter& c, int i)
{
  for (int k = 0; k <= i; ++k) {
    c.Record(kHH, 1 * CLHEP::picosecond);
  }
  if (i == 1) {
    c.Record(kEE, 100 * CLHEP::picosecond);
  }
  c.CommitEvent();
}

static void TestMergeEqualsSingleFeed()
{
  ReactionCounter single, a, b;
  for (int i = 0; i < 4; ++i) {
    FeedEvent(single, i);
    FeedEvent((i % 2 == 0) ? a : b, i);
  }
  a.Merge(b);

  assert(a.CommittedEvents() == single.CommittedEvents());
  assert(a.GetStats().size() == single.GetStats().size());
  for (const auto& [bin, reactions] : single.GetStats()) {
    for (const auto& [label, stat] : reactions) {
      const EventStat& merged = a.GetStats().at(bin).at(label);
      assert(merged.Sum() == stat.Sum());
      assert(merged.SumSq() == stat.SumSq());
    }
  }
}

static void TestMergeAddsEntriesOnlyPresentInOther()
{
  ReactionCounter a, b;
  b.Record("H3Op + OHm -> (no products)", 100 * CLHEP::picosecond);
  b.CommitEvent();

  a.Merge(b);

  assert(a.CommittedEvents() == 1);
  assert(a.GetStats().at(100 * CLHEP::picosecond).at("H3Op + OHm -> (no products)").Sum() == 1.0);
}

static void TestMergeDoesNotModifyOtherAndIgnoresItsOpenEvent()
{
  ReactionCounter a, b;
  b.Record(kHH, 1 * CLHEP::picosecond);
  b.CommitEvent();
  b.Record(kHH, 1 * CLHEP::picosecond);  // open event, not committed

  a.Merge(b);

  assert(b.GetStats().at(1 * CLHEP::picosecond).at(kHH).Sum() == 1.0);
  assert(b.CommittedEvents() == 1);
  assert(a.GetStats().at(1 * CLHEP::picosecond).at(kHH).Sum() == 1.0);
}

static void TestSelfMergeDoubles()
{
  ReactionCounter a;
  a.Record(kHH, 1 * CLHEP::picosecond);
  a.CommitEvent();

  a.Merge(a);

  assert(a.CommittedEvents() == 2);
  assert(a.GetStats().at(1 * CLHEP::picosecond).at(kHH).Sum() == 2.0);
}

// --- Clear -------------------------------------------------------------

static void TestClearEmptiesEverything()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.CommitEvent();
  c.Record(kHH, 1 * CLHEP::picosecond);

  c.Clear();
  c.CommitEvent();  // the open event was dropped

  assert(c.GetStats().empty());
  assert(c.CommittedEvents() == 1);
}

// --- ConfigureBinEdges -----------------------------------------------------

static void TestConfigureBinEdgesChangesBinFor()
{
  ReactionCounter::ConfigureBinEdges({5 * CLHEP::picosecond, 50 * CLHEP::picosecond});

  assert(ReactionCounter::BinFor(1 * CLHEP::picosecond) == 5 * CLHEP::picosecond);
  assert(ReactionCounter::BinFor(10 * CLHEP::picosecond) == 50 * CLHEP::picosecond);
  assert(ReactionCounter::BinFor(1000 * CLHEP::picosecond) == 50 * CLHEP::picosecond);

  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());
}

static void TestConfigureBinEdgesSortsUnsortedInput()
{
  ReactionCounter::ConfigureBinEdges({50 * CLHEP::picosecond, 5 * CLHEP::picosecond});

  assert(ReactionCounter::BinFor(1 * CLHEP::picosecond) == 5 * CLHEP::picosecond);

  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());
}

static void TestConfigureBinEdgesEmptyIsNoOp()
{
  ReactionCounter::ConfigureBinEdges({5 * CLHEP::picosecond});
  ReactionCounter::ConfigureBinEdges({});

  assert(ReactionCounter::BinFor(1 * CLHEP::picosecond) == 5 * CLHEP::picosecond);

  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());
}

static void TestConfigureBinEdgesRestoresDefault()
{
  ReactionCounter::ConfigureBinEdges({5 * CLHEP::picosecond});
  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());

  assert(ReactionCounter::BinFor(10 * CLHEP::picosecond) == 10 * CLHEP::picosecond);
}

// --- ParseBinEdgesList ---------------------------------------------------

static void TestParseBinEdgesListParsesValuesAndUnit()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("1 10 100 picosecond", edges, error);

  assert(ok);
  assert(edges.size() == 3);
  assert(edges[0] == 1 * CLHEP::picosecond);
  assert(edges[1] == 10 * CLHEP::picosecond);
  assert(edges[2] == 100 * CLHEP::picosecond);
}

static void TestParseBinEdgesListRejectsTooFewTokens()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("picosecond", edges, error);

  assert(!ok);
  assert(!error.empty());
  assert(edges.empty());
}

static void TestParseBinEdgesListRejectsNonNumericEdge()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("abc 10 picosecond", edges, error);

  assert(!ok);
  assert(!error.empty());
}

static void TestParseBinEdgesListRejectsEmptyString()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("", edges, error);

  assert(!ok);
  assert(!error.empty());
}

static void TestParseBinEdgesListRejectsUnknownUnit()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("1 10 meter", edges, error);

  assert(!ok);
  assert(!error.empty());
}

static void TestParseBinEdgesListAcceptsUnitSymbol()
{
  std::vector<G4double> edges;
  G4String error;

  G4bool ok = ReactionCounter::ParseBinEdgesList("1 10 ps", edges, error);

  assert(ok);
  assert(edges.size() == 2);
  assert(edges[0] == 1 * CLHEP::picosecond);
  assert(edges[1] == 10 * CLHEP::picosecond);
}

// --- LowerEdgeFor ----------------------------------------------------------

static void TestLowerEdgeFor()
{
  assert(ReactionCounter::LowerEdgeFor(1 * CLHEP::picosecond) == 0.);
  assert(ReactionCounter::LowerEdgeFor(10 * CLHEP::picosecond) == 1 * CLHEP::picosecond);
  assert(ReactionCounter::LowerEdgeFor(999999 * CLHEP::picosecond) == 100000 * CLHEP::picosecond);
}

// --- BuildReactionIdMap ------------------------------------------------

static void TestBuildReactionIdMapAssignsSortedIds()
{
  ReactionCounter counter;
  counter.Record(kHH, 1 * CLHEP::picosecond);
  counter.Record(kEE, 10 * CLHEP::picosecond);
  counter.CommitEvent();

  auto idMap = counter.BuildReactionIdMap();

  assert(idMap.size() == 2);
  assert(idMap.at(kHH) == 0);
  assert(idMap.at(kEE) == 1);
}

static void TestBuildReactionIdMapDeduplicatesAcrossBins()
{
  ReactionCounter counter;
  counter.Record(kHH, 1 * CLHEP::picosecond);
  counter.Record(kHH, 10 * CLHEP::picosecond);
  counter.CommitEvent();

  auto idMap = counter.BuildReactionIdMap();

  assert(idMap.size() == 1);
  assert(idMap.at(kHH) == 0);
}

// --- ToTables --------------------------------------------------------------

static std::int64_t AsInt(const OutputTable::Cell& cell) { return std::get<std::int64_t>(cell); }
static double AsDouble(const OutputTable::Cell& cell) { return std::get<double>(cell); }
static std::string AsString(const OutputTable::Cell& cell) { return std::get<std::string>(cell); }
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-15; }

static void TestToTablesSchemaAndRows()
{
  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kHH, 1 * CLHEP::picosecond);
  c.Record(kEE, 10 * CLHEP::picosecond);
  c.CommitEvent();
  c.CommitEvent();  // empty event: nEvents = 2, zero-filled

  const std::vector<OutputTable> tables = c.ToTables();
  assert(tables.size() == 2);

  const OutputTable& reactions = tables[0];
  assert(reactions.Name() == "reactions");
  const char* expected[] = {"reactionId", "binStart_ns", "binEnd_ns", "nEvents",
                            "countSum",   "countSumSq",  "countMean", "countStdErr"};
  assert(reactions.Columns().size() == 8);
  for (std::size_t i = 0; i < 8; ++i) {
    assert(reactions.Columns()[i].name == expected[i]);
  }
  assert(reactions.Rows().size() == 2);

  // Ordered by bin: 1 ps (kHH, id 0) then 10 ps (kEE, id 1).
  const OutputTable::Row& r0 = reactions.Rows()[0];
  assert(AsInt(r0[0]) == 0);
  assert(AsDouble(r0[1]) == 0.0);  // first bin starts at 0
  assert(Near(AsDouble(r0[2]), 1e-3));  // 1 ps = 1e-3 ns
  assert(AsInt(r0[3]) == 2);
  assert(AsDouble(r0[4]) == 2.0);
  assert(AsDouble(r0[5]) == 4.0);
  assert(AsDouble(r0[6]) == 1.0);  // (2 + 0) / 2

  const OutputTable::Row& r1 = reactions.Rows()[1];
  assert(AsInt(r1[0]) == 1);
  assert(Near(AsDouble(r1[1]), 1e-3));  // previous edge, 1 ps
  assert(Near(AsDouble(r1[2]), 1e-2));  // 10 ps

  const OutputTable& catalog = tables[1];
  assert(catalog.Name() == "reactionCatalog");
  assert(catalog.Columns().size() == 2);
  assert(catalog.Columns()[0].name == "reactionId");
  assert(catalog.Columns()[1].name == "reaction");
  assert(catalog.Rows().size() == 2);
  assert(AsInt(catalog.Rows()[0][0]) == 0 && AsString(catalog.Rows()[0][1]) == kHH);
  assert(AsInt(catalog.Rows()[1][0]) == 1 && AsString(catalog.Rows()[1][1]) == kEE);
}

static void TestToTablesBinStartEndForFixedBinning()
{
  // Fixed 5 ps width up to 20 ps.
  ReactionCounter::ConfigureBinEdges({5 * CLHEP::picosecond, 10 * CLHEP::picosecond,
                                      15 * CLHEP::picosecond, 20 * CLHEP::picosecond});
  ReactionCounter c;
  c.Record(kHH, 3 * CLHEP::picosecond);   // bin (0, 5]
  c.Record(kHH, 12 * CLHEP::picosecond);  // bin (10, 15]
  c.CommitEvent();

  const OutputTable reactions = c.ToTables()[0];
  assert(reactions.Rows().size() == 2);
  assert(AsDouble(reactions.Rows()[0][1]) == 0.0);
  assert(Near(AsDouble(reactions.Rows()[0][2]), 5e-3));
  assert(Near(AsDouble(reactions.Rows()[1][1]), 10e-3));
  assert(Near(AsDouble(reactions.Rows()[1][2]), 15e-3));

  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());
}

static void TestToTablesBinStartEndForListBinning()
{
  std::vector<G4double> edges;
  G4String error;
  assert(ReactionCounter::ParseBinEdgesList("2 7 100 ps", edges, error));
  ReactionCounter::ConfigureBinEdges(edges);

  ReactionCounter c;
  c.Record(kHH, 1 * CLHEP::picosecond);     // (0, 2]
  c.Record(kHH, 50 * CLHEP::picosecond);    // (7, 100]
  c.Record(kHH, 5000 * CLHEP::picosecond);  // clamps to the last bin (7, 100]
  c.CommitEvent();

  const OutputTable reactions = c.ToTables()[0];
  assert(reactions.Rows().size() == 2);
  assert(AsDouble(reactions.Rows()[0][1]) == 0.0);
  assert(Near(AsDouble(reactions.Rows()[0][2]), 2e-3));
  assert(Near(AsDouble(reactions.Rows()[1][1]), 7e-3));
  assert(Near(AsDouble(reactions.Rows()[1][2]), 100e-3));
  assert(AsDouble(reactions.Rows()[1][4]) == 2.0);  // both firings in one bin

  ReactionCounter::ConfigureBinEdges(ReactionCounter::DefaultBinEdges());
}

static void TestToTablesEmptyHasSchemaNoRows()
{
  ReactionCounter c;
  c.CommitEvent();
  const std::vector<OutputTable> tables = c.ToTables();
  assert(tables.size() == 2);
  assert(tables[0].Rows().empty());
  assert(tables[1].Rows().empty());
  assert(tables[0].Columns().size() == 8);
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

  TestBinForExactEdgeReturnsThatEdge();
  TestBinForBetweenEdgesReturnsNextEdge();
  TestBinForBelowFirstEdgeReturnsFirstEdge();
  TestBinForAboveLastEdgeClampsToLastEdge();

  TestCommitFoldsPerEvent();
  TestRecordDifferentBinsStaySeparate();
  TestUncommittedEventNotCounted();
  TestZeroFill();

  TestMergeEqualsSingleFeed();
  TestMergeAddsEntriesOnlyPresentInOther();
  TestMergeDoesNotModifyOtherAndIgnoresItsOpenEvent();
  TestSelfMergeDoubles();

  TestClearEmptiesEverything();

  TestConfigureBinEdgesChangesBinFor();
  TestConfigureBinEdgesSortsUnsortedInput();
  TestConfigureBinEdgesEmptyIsNoOp();
  TestConfigureBinEdgesRestoresDefault();

  TestParseBinEdgesListParsesValuesAndUnit();
  TestParseBinEdgesListRejectsTooFewTokens();
  TestParseBinEdgesListRejectsNonNumericEdge();
  TestParseBinEdgesListRejectsEmptyString();
  TestParseBinEdgesListRejectsUnknownUnit();
  TestParseBinEdgesListAcceptsUnitSymbol();

  TestLowerEdgeFor();

  TestBuildReactionIdMapAssignsSortedIds();
  TestBuildReactionIdMapDeduplicatesAcrossBins();

  TestToTablesSchemaAndRows();
  TestToTablesBinStartEndForFixedBinning();
  TestToTablesBinStartEndForListBinning();
  TestToTablesEmptyHasSchemaNoRows();

  std::cout << "All ReactionCounter tests passed." << std::endl;
  return 0;
}
