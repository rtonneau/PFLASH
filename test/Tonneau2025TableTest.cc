/// \file Tonneau2025TableTest.cc
/// \brief Plain-assert unit tests for Tonneau2025Table and the Geant4-free part
/// of Tonneau2025Reactions (routing, types, bulk-reaction list). No Geant4
/// runtime: the reaction-table builder and HO3 creation need a live molecule
/// table and are checked by the smoke run.
#include "chemistry/catalog/Tonneau2025Reactions.hh"
#include "chemistry/catalog/Tonneau2025Table.hh"

#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace Tonneau2025Table;
namespace TR = Tonneau2025Reactions;

namespace
{
bool Near(double a, double b) { return std::fabs(a - b) <= 1e-12 * std::fabs(b); }

bool Is(const std::vector<Term>& terms, const std::vector<Term>& expected)
{
  if (terms.size() != expected.size()) {
    return false;
  }
  for (std::size_t i = 0; i < terms.size(); ++i) {
    if (terms[i].species != expected[i].species || terms[i].count != expected[i].count) {
      return false;
    }
  }
  return true;
}
}  // namespace

static void TestCountAndIds()
{
  const auto& table = Reactions();
  assert(table.size() == 73);
  std::set<int> ids;
  for (std::size_t i = 0; i < table.size(); ++i) {
    assert(table[i].id == static_cast<int>(i) + 1);  // table order, R1..R73
    ids.insert(table[i].id);
    assert(Find(table[i].id) == &table[i]);
  }
  assert(ids.size() == 73);
  assert(Find(0) == nullptr);
  assert(Find(74) == nullptr);
}

static void TestEveryRowIsWellFormed()
{
  for (const auto& r : Reactions()) {
    assert(!r.reactants.empty());
    assert(!r.products.empty());
    assert(r.reactants.size() <= 3);
    assert(r.k > 0.0);
    assert(r.source >= 'A' && r.source <= 'D');
    for (const auto& t : r.reactants) {
      assert(!t.species.empty() && t.count >= 1);
    }
    for (const auto& t : r.products) {
      assert(!t.species.empty() && t.count >= 1);
    }
  }
}

static void TestPinnedRates()
{
  assert(Near(Find(1)->k, 5.50e9) && Find(1)->source == 'B');
  assert(Near(Find(5)->k, 1.10e10) && Find(5)->source == 'C');
  assert(Near(Find(12)->k, 2.20e7) && Find(12)->source == 'D');
  // The two-line rows of the table (R20..R37), checked against the page image.
  assert(Near(Find(20)->k, 3.95e7));
  assert(Near(Find(21)->k, 8.84e9));
  assert(Near(Find(22)->k, 8.00e9) && Find(22)->source == 'B');
  assert(Near(Find(23)->k, 11.0) && Find(23)->unit == Unit::FirstOrder);
  assert(Near(Find(24)->k, 7.52e9));
  assert(Near(Find(29)->k, 1.14e10));
  assert(Near(Find(30)->k, 3.80e10));
  assert(Near(Find(34)->k, 8.40e5));
  assert(Near(Find(36)->k, 0.50));
  assert(Near(Find(37)->k, 0.50));
  assert(Near(Find(41)->k, 0.3));
  assert(Near(Find(53)->k, 2.62e3) && Find(53)->unit == Unit::FirstOrder);
  assert(Near(Find(58)->k, 2.11e-5) && Find(58)->unit == Unit::ZeroOrder);
  assert(Near(Find(59)->k, 1.18e11) && Find(59)->unit == Unit::SecondOrder);
  assert(Near(Find(66)->k, 19.0) && Find(66)->source == 'D');
  assert(Near(Find(73)->k, 1.27e6) && Find(73)->unit == Unit::FirstOrder);
}

static void TestPinnedSpecies()
{
  const Reaction& r7 = *Find(7);
  assert(Is(r7.reactants, {{"e-aq"}, {"O2"}}));
  assert(Is(r7.products, {{"O2-"}}));

  const Reaction& r1 = *Find(1);
  assert(Is(r1.reactants, {{"e-aq", 2}, {"H2O", 2}}));
  assert(Is(r1.products, {{"H2"}, {"OH-", 2}}));

  assert(Is(Find(19)->reactants, {{"OH.", 2}}));
  assert(Is(Find(19)->products, {{"H2O2"}}));
  assert(Is(Find(34)->reactants, {{"HO2.", 2}}));

  assert(Is(Find(16)->products, {{"O2-", 2}, {"H3O+"}}));
  assert(Is(Find(31)->products, {{"HO2."}}));
  assert(Is(Find(43)->reactants, {{"O2-"}, {"HO2-"}}));
  assert(Is(Find(43)->products, {{"O-"}, {"O2"}, {"OH-"}}));
  assert(Is(Find(58)->reactants, {{"H2O", 2}}));
  assert(Is(Find(58)->products, {{"H3O+"}, {"OH-"}}));
  assert(Is(Find(73)->reactants, {{"O-"}, {"H2O"}}));
  assert(Is(Find(73)->products, {{"OH."}, {"OH-"}}));
}

static void TestBulkMarking()
{
  std::set<int> expected = {1, 23};
  for (int i = 58; i <= 73; ++i) {
    expected.insert(i);
  }
  std::set<int> marked;
  for (const auto& r : Reactions()) {
    if (r.bulk) {
      marked.insert(r.id);
    }
  }
  assert(marked == expected);
}

static void TestUnitLabels()
{
  assert(std::string(UnitLabel(Unit::SecondOrder)) == "M^-1 s^-1");
  assert(std::string(UnitLabel(Unit::FirstOrder)) == "s^-1");
  assert(std::string(UnitLabel(Unit::ZeroOrder)) == "M s^-1");
}

// ---- Tonneau2025Reactions (Geant4-free part) ----

namespace
{
const std::string kOH = std::string("\xC2\xB0") + "OH";
const std::string kHO2 = std::string("HO2") + "\xC2\xB0";
const double kM = 1e-3 * m3 / (mole * s);  // M^-1 s^-1 in Geant4 units

bool InTable(TR::Route r)
{
  return r == TR::Route::Table || r == TR::Route::TableAndBulk || r == TR::Route::WaterTable ||
         r == TR::Route::BulkAndWaterTable;
}
bool InBulk(TR::Route r)
{
  return r == TR::Route::TableAndBulk || r == TR::Route::Bulk ||
         r == TR::Route::BulkAndWaterTable;
}

const ChemistryTypes::BulkReaction* FindBulk(const ChemistryTypes::BulkReactionList& list,
                                             const std::string& molecule,
                                             const std::string& partner)
{
  for (const auto& entry : list) {
    if (entry.molecule != molecule) {
      continue;
    }
    for (const auto& r : entry.reactions) {
      if (r.partner == partner) {
        return &r;
      }
    }
  }
  return nullptr;
}

bool SameProducts(std::vector<std::string> a, std::vector<std::string> b)
{
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}
}  // namespace

static void TestPlacementsCoverEveryRow()
{
  const auto& placements = TR::Placements();
  assert(placements.size() == 73);
  int inTable = 0;
  std::set<int> bulkRows;
  for (std::size_t i = 0; i < placements.size(); ++i) {
    const auto& p = placements[i];
    assert(p.id == static_cast<int>(i) + 1);
    assert(InTable(p.route) == (p.tableType >= 0));
    assert(InBulk(p.route) == (p.bulkType >= 0));
    assert(p.tableType >= -1 && p.tableType <= 1);
    assert(p.bulkType == -1 || p.bulkType == 0 || (p.bulkType >= 6 && p.bulkType <= 8));
    // Every row except the buffer row R58 is a reaction somewhere.
    assert(p.route == TR::Route::Buffer || InTable(p.route) || InBulk(p.route));
    inTable += InTable(p.route) ? 1 : 0;
    if (InBulk(p.route)) {
      bulkRows.insert(p.id);
    }
  }
  assert(inTable == 63);
  assert(placements[57].route == TR::Route::Buffer);              // R58
  assert(placements[22].route == TR::Route::WaterTable);          // R23
  assert(placements[70].route == TR::Route::BulkAndWaterTable);   // R71

  // Acid-base block R56..R73 (R58 excepted), R53..R55, and the O2 rows.
  std::set<int> expected = {7, 31, 47, 53, 54, 55};
  for (int id = 56; id <= 73; ++id) {
    if (id != 58) {
      expected.insert(id);
    }
  }
  assert(bulkRows == expected);
}

static void TestPinnedReactionTypes()
{
  const auto& placements = TR::Placements();
  // Type 0: PureWater's fully diffusion-controlled pairs (R1, R2, R24, R26,
  // R41, R54, R59), the H + H2O entries (R23, R71), R13 (k > k_diff: the
  // deviation from PureWater) and R30 (k > k_diff, no PureWater pair).
  const std::set<int> typeZero = {1, 2, 13, 23, 24, 26, 30, 41, 54, 59, 71};
  for (const auto& p : placements) {
    if (p.tableType < 0) {
      continue;
    }
    assert(p.tableType == (typeZero.count(p.id) != 0 ? 0 : 1));
  }
  // Bulk types copied from PureWater's (molecule, partner) pairs.
  const std::map<int, int> equilibrium = {{64, 6}, {65, 6}, {68, 7}, {69, 7}, {72, 8}, {73, 8}};
  for (const auto& p : placements) {
    if (p.bulkType < 0) {
      continue;
    }
    const auto it = equilibrium.find(p.id);
    assert(p.bulkType == (it != equilibrium.end() ? it->second : 0));
  }
}

static void TestSpeciesNames()
{
  assert(TR::SpeciesName("H2O").empty());
  assert(TR::SpeciesName("e-aq") == "e_aq");
  assert(TR::SpeciesName("OH.") == kOH);
  assert(TR::SpeciesName("HO2.") == kHO2);
  assert(TR::SpeciesName("HO2-") == "HO2m");
  assert(TR::SpeciesName("O3-") == "O3m");
  assert(TR::SpeciesName("HO3") == "HO3");
  assert(TR::SpeciesName("XYZ") == "?XYZ");
  for (const auto& r : Reactions()) {
    for (const auto* side : {&r.reactants, &r.products}) {
      for (const auto& t : *side) {
        assert(TR::SpeciesName(t.species).rfind('?', 0) != 0);
      }
    }
  }
}

static void TestBulkReactionList()
{
  const auto list = TR::BuildTonneau2025BulkReactions();
  const std::set<std::string> partners = {"H2O", "H3Op(B)", "OHm(B)", "O2"};
  std::set<std::string> molecules;
  std::size_t total = 0;
  for (const auto& entry : list) {
    // One G4DNAScavengerProcess per molecule, one reaction per partner.
    assert(molecules.insert(entry.molecule).second);
    std::set<std::string> seen;
    for (const auto& r : entry.reactions) {
      assert(partners.count(r.partner) == 1);
      assert(seen.insert(r.partner).second);
      assert(r.rate > 0.);
      for (const auto& p : r.products) {
        assert(!p.empty() && p[0] != '?');
        assert(p != "H3Op" && p != "OHm");  // bulk-made ions join the buffer
      }
      ++total;
    }
  }
  // 23 bulk rows, R59 giving two entries.
  assert(total == 24);

  // O2 rows: same rate and products as their reaction-table entry.
  const auto* r7 = FindBulk(list, "e_aq", "O2");
  assert(r7 != nullptr && Near(r7->rate, 1.90e10 * kM) && SameProducts(r7->products, {"O2m"}));
  const auto* r31 = FindBulk(list, "H", "O2");
  assert(r31 != nullptr && Near(r31->rate, 1.31e10 * kM) && SameProducts(r31->products, {kHO2}));
  const auto* r47 = FindBulk(list, "Om", "O2");
  assert(r47 != nullptr && Near(r47->rate, 3.75e9 * kM) && SameProducts(r47->products, {"O3m"}));

  // Acid-base against the buffer; R59 both ways.
  const auto* r59a = FindBulk(list, "OHm", "H3Op(B)");
  const auto* r59b = FindBulk(list, "H3Op", "OHm(B)");
  assert(r59a != nullptr && r59b != nullptr && r59a->products.empty() && r59b->products.empty());
  assert(Near(r59a->rate, 1.18e11 * kM) && Near(r59b->rate, 1.18e11 * kM));
  const auto* r65 = FindBulk(list, "O2m", "H3Op(B)");
  assert(r65 != nullptr && r65->reactionType == 6 && SameProducts(r65->products, {kHO2}));
  const auto* r54 = FindBulk(list, "O3m", "H3Op(B)");
  assert(r54 != nullptr && SameProducts(r54->products, {"O2", kOH}));

  // First order against H2O: water reactions and the two decays.
  const auto* r71 = FindBulk(list, "H", "H2O");
  assert(r71 != nullptr && Near(r71->rate, 5.83 / s));
  assert(SameProducts(r71->products, {"e_aq", "H3Op(B)"}));
  const auto* r64 = FindBulk(list, kHO2, "H2O");
  assert(r64 != nullptr && r64->reactionType == 6 && Near(r64->rate, 7.73e5 / s));
  assert(SameProducts(r64->products, {"H3Op(B)", "O2m"}));
  const auto* r53 = FindBulk(list, "O3m", "H2O");
  assert(r53 != nullptr && Near(r53->rate, 2.62e3 / s) && SameProducts(r53->products, {"O2", "Om"}));
  const auto* r55 = FindBulk(list, "HO3", "H2O");
  assert(r55 != nullptr && Near(r55->rate, 1.10e5 / s) && SameProducts(r55->products, {"O2", kOH}));
}

static void TestTypeOneSpeciesHaveRadii()
{
  // Configurations given a vdW radius by G4ChemDissociationChannels_option1,
  // plus HO3 (ConstructTonneau2025Molecules). BuildTonneau2025Reactions also
  // checks the live radii and fails fatally on a missing one.
  const std::set<std::string> withRadius = {"e_aq", "H",  "H2",  kOH,  "OHm", "H3Op",
                                            "H2O2", kHO2, "HO2m", "Oxy", "Om", "O2",
                                            "O2m",  "O3", "O3m", "HO3"};
  const auto species = TR::TypeOneSpecies();
  assert(!species.empty());
  for (const auto& name : species) {
    assert(withRadius.count(name) == 1);
  }
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestCountAndIds();
  TestEveryRowIsWellFormed();
  TestPinnedRates();
  TestPinnedSpecies();
  TestBulkMarking();
  TestUnitLabels();
  TestPlacementsCoverEveryRow();
  TestPinnedReactionTypes();
  TestSpeciesNames();
  TestBulkReactionList();
  TestTypeOneSpeciesHaveRadii();
  std::cout << "Tonneau2025TableTest: all tests passed\n";
  return 0;
}
