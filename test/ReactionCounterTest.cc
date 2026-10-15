/// \file ReactionCounterTest.cc
/// \brief Plain-assert unit tests for ReactionCounter (no test framework, no
/// Geant4 runtime -- exercises pure accumulation/binning/merge logic only).

#include "scoring/ReactionCounter.hh"

#include "G4SystemOfUnits.hh"

#include <cassert>
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

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

// --- Record / GetCounts ---------------------------------------------------

static void TestRecordSingleReactionIncrementsCorrectBin()
{
  ReactionCounter counter;
  counter.Record("e_aq + e_aq -> H2 + OHm + OHm", 10 * CLHEP::picosecond);

  const auto& counts = counter.GetCounts();
  auto binIt = counts.find(10 * CLHEP::picosecond);
  assert(binIt != counts.end());
  auto reactionIt = binIt->second.find("e_aq + e_aq -> H2 + OHm + OHm");
  assert(reactionIt != binIt->second.end());
  assert(reactionIt->second == 1);
}

static void TestRecordSameReactionTwiceAccumulates()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);

  assert(counter.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 2);
}

static void TestRecordDifferentReactionsStaySeparate()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);
  counter.Record("e_aq + e_aq -> H2 + OHm + OHm", 1 * CLHEP::picosecond);

  const auto& bin = counter.GetCounts().at(1 * CLHEP::picosecond);
  assert(bin.at("H + H -> H2") == 1);
  assert(bin.at("e_aq + e_aq -> H2 + OHm + OHm") == 1);
}

// --- Merge -----------------------------------------------------------------

static void TestMergeSumsOverlappingEntries()
{
  ReactionCounter a;
  a.Record("H + H -> H2", 1 * CLHEP::picosecond);
  ReactionCounter b;
  b.Record("H + H -> H2", 1 * CLHEP::picosecond);
  b.Record("H + H -> H2", 1 * CLHEP::picosecond);

  a.Merge(b);

  assert(a.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 3);
}

static void TestMergeAddsEntriesOnlyPresentInOther()
{
  ReactionCounter a;
  ReactionCounter b;
  b.Record("H3Op + OHm -> (no products)", 100 * CLHEP::picosecond);

  a.Merge(b);

  assert(a.GetCounts().at(100 * CLHEP::picosecond).at("H3Op + OHm -> (no products)") == 1);
}

static void TestMergeDoesNotModifyOther()
{
  ReactionCounter a;
  ReactionCounter b;
  b.Record("H + H -> H2", 1 * CLHEP::picosecond);

  a.Merge(b);

  assert(b.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 1);
}

// --- Clear -------------------------------------------------------------

static void TestClearEmptiesCounts()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);

  counter.Clear();

  assert(counter.GetCounts().empty());
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

// --- BuildReactionIdMap ------------------------------------------------

static void TestBuildReactionIdMapAssignsSortedIds()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);
  counter.Record("e_aq + e_aq -> H2 + OHm + OHm", 10 * CLHEP::picosecond);

  auto idMap = counter.BuildReactionIdMap();

  assert(idMap.size() == 2);
  assert(idMap.at("H + H -> H2") == 0);
  assert(idMap.at("e_aq + e_aq -> H2 + OHm + OHm") == 1);
}

static void TestBuildReactionIdMapDeduplicatesAcrossBins()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);
  counter.Record("H + H -> H2", 10 * CLHEP::picosecond);

  auto idMap = counter.BuildReactionIdMap();

  assert(idMap.size() == 1);
  assert(idMap.at("H + H -> H2") == 0);
}

// --- WriteAscii --------------------------------------------------------

static void TestWriteAsciiFormatsBinThenReactionCountLines()
{
  ReactionCounter counter;
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);
  counter.Record("H + H -> H2", 1 * CLHEP::picosecond);

  std::ostringstream out;
  counter.WriteAscii(out);

  G4String text = out.str();
  assert(text.find("H + H -> H2    count = 2") != G4String::npos);
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

  TestRecordSingleReactionIncrementsCorrectBin();
  TestRecordSameReactionTwiceAccumulates();
  TestRecordDifferentReactionsStaySeparate();

  TestMergeSumsOverlappingEntries();
  TestMergeAddsEntriesOnlyPresentInOther();
  TestMergeDoesNotModifyOther();

  TestClearEmptiesCounts();

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

  TestBuildReactionIdMapAssignsSortedIds();
  TestBuildReactionIdMapDeduplicatesAcrossBins();

  TestWriteAsciiFormatsBinThenReactionCountLines();

  std::cout << "All ReactionCounter tests passed." << std::endl;
  return 0;
}
