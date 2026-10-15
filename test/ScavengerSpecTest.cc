/// \file ScavengerSpecTest.cc
/// \brief Plain-assert unit tests for ScavengerSpec (no Geant4 runtime).
#include "geometry/ScavengerSpec.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
bool Near(double a, double b)
{
  return std::fabs(a - b) <= 1e-12 * std::fabs(b);
}

bool Contains(const std::string& text, const std::string& part)
{
  return text.find(part) != std::string::npos;
}

ScavengerSpec::Entry MustParse(const std::string& text)
{
  ScavengerSpec::Entry entry;
  std::string err;
  const bool ok = ScavengerSpec::Parse(text, entry, err);
  if (!ok) {
    std::cerr << "unexpected parse failure for '" << text << "': " << err << "\n";
  }
  assert(ok);
  (void)ok;
  return entry;
}

std::string MustFail(const std::string& text)
{
  ScavengerSpec::Entry entry;
  std::string err;
  const bool ok = ScavengerSpec::Parse(text, entry, err);
  assert(!ok);
  assert(!err.empty());
  (void)ok;
  return err;
}
}  // namespace

static void TestUnitsConvertToMolarity()
{
  assert(Near(MustParse("O2 21 %").molarity, 0.21 * 0.0013));
  assert(Near(MustParse("O2 100 %").molarity, 0.0013));
  assert(Near(MustParse("H2O2 1 mM").molarity, 1e-3));
  assert(Near(MustParse("H2O2 2 uM").molarity, 2e-6));
  assert(Near(MustParse("H2O2 0.5 M").molarity, 0.5));
  assert(MustParse("O2 21 %").species == "O2");
}

static void TestZeroIsAccepted()
{
  assert(MustParse("O2 0 %").molarity == 0.);
}

static void TestRejectsBadValuesAndUnits()
{
  assert(Contains(MustFail("O2 -1 %"), "negative"));
  assert(Contains(MustFail("O2 21 mol"), "unknown unit"));
  assert(Contains(MustFail("O2 abc %"), "invalid concentration"));
  assert(Contains(MustFail("O2 1x %"), "invalid concentration"));
  assert(Contains(MustFail("O2 nan M"), "invalid concentration"));
  MustFail("O2 21");
  MustFail("O2 21 % extra");
  MustFail("");
}

static void TestPercentIsO2Only()
{
  assert(Contains(MustFail("CO2 1 %"), "only defined for O2"));
  // Wrong case: names are case-sensitive, so "o2" is not O2.
  assert(Contains(MustFail("o2 21 %"), "O2"));
}

static void TestPhOwnedSpeciesRejected()
{
  for (const char* name : {"H2O", "H2O(B)", "H3Op(B)", "OHm(B)"}) {
    assert(ScavengerSpec::IsPhOwned(name));
    assert(Contains(MustFail(std::string(name) + " 1 M"), "/chem/env/pH"));
  }
  assert(!ScavengerSpec::IsPhOwned("O2"));
  assert(!ScavengerSpec::IsPhOwned("H3Op"));  // tracked H3O+, not the bulk pool
}

static void TestUpsertLastWins()
{
  ScavengerSpec::List list;
  ScavengerSpec::Upsert(list, {"O2", 1.0});
  ScavengerSpec::Upsert(list, {"H2O2", 2.0});
  ScavengerSpec::Upsert(list, {"O2", 3.0});
  assert(list.size() == 2);
  assert(list[0].species == "O2" && list[0].molarity == 3.0);
  assert(list[1].species == "H2O2" && list[1].molarity == 2.0);
}

static void TestInertSpecies()
{
  const ChemistryTypes::BulkReactionList reactions = {
    {"e_aq", {{"O2", 1.0, {"O2m"}, 0}, {"H3Op(B)", 1.0, {"H"}, 0}}},
  };
  ScavengerSpec::List list;
  ScavengerSpec::Upsert(list, {"O2", 2.7e-4});
  ScavengerSpec::Upsert(list, {"H2O2", 1e-3});
  ScavengerSpec::Upsert(list, {"NO3m", 1e-3});
  ScavengerSpec::Upsert(list, {"NO3m", 0.});  // set back to 0 -> absent, not inert
  const auto inert = ScavengerSpec::InertSpecies(list, reactions);
  assert(inert.size() == 1);
  assert(inert[0] == "H2O2");
  assert(ScavengerSpec::InertSpecies(list, {}).size() == 2);  // Chemistry without bulk reactions
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestUnitsConvertToMolarity();
  TestZeroIsAccepted();
  TestRejectsBadValuesAndUnits();
  TestPercentIsO2Only();
  TestPhOwnedSpeciesRejected();
  TestUpsertLastWins();
  TestInertSpecies();
  std::cout << "ScavengerSpecTest: all tests passed\n";
  return 0;
}
