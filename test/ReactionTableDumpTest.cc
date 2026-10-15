/// \file ReactionTableDumpTest.cc
/// \brief Plain-assert unit tests for ReactionTableDump::FormatReactionLabel
/// (no test framework, no Geant4 runtime -- exercises pure string-formatting
/// logic only).

#include "chemistry/ReactionTableDump.hh"

#include <cassert>
#include <iostream>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static void TestTwoReactantsTwoProducts()
{
  G4String label = ReactionTableDump::FormatReactionLabel(
      "e_aq", "e_aq", std::vector<G4String>{"H2", "OHm", "OHm"});
  assert(label == "e_aq + e_aq -> H2 + OHm + OHm");
}

static void TestSingleProduct()
{
  G4String label =
      ReactionTableDump::FormatReactionLabel("H", "H", std::vector<G4String>{"H2"});
  assert(label == "H + H -> H2");
}

static void TestNoProducts()
{
  G4String label =
      ReactionTableDump::FormatReactionLabel("H3Op", "OHm", std::vector<G4String>{});
  assert(label == "H3Op + OHm -> (no products)");
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

  TestTwoReactantsTwoProducts();
  TestSingleProduct();
  TestNoProducts();

  std::cout << "All ReactionTableDump tests passed." << std::endl;
  return 0;
}
