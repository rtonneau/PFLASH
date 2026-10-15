/// ile Tonneau2025Table.cc
/// rief Table 2 of Tonneau et al. 2025 as plain data (see Tonneau2025Table.hh
/// for the conventions and the list of remaining uncertainties)

#include "chemistry/catalog/Tonneau2025Table.hh"

namespace Tonneau2025Table
{
namespace
{
std::vector<Reaction> MakeReactions()
{
  // {id, reactants, products, k, unit, source, bulk}
  return {
      {1, {{"e-aq", 2}, {"H2O", 2}}, {{"H2"}, {"OH-", 2}}, 5.50e9, Unit::SecondOrder, 'B', true},
      {2, {{"e-aq"}, {"H."}, {"H2O"}}, {{"H2"}, {"OH-"}}, 2.76e10, Unit::SecondOrder, 'A', false},
      {3, {{"e-aq"}, {"OH."}}, {{"OH-"}}, 3.00e10, Unit::SecondOrder, 'B', false},
      {4, {{"e-aq"}, {"O-"}, {"H2O"}}, {{"OH-", 2}}, 2.20e10, Unit::SecondOrder, 'A', false},
      {5, {{"e-aq"}, {"H2O2"}}, {{"OH."}, {"OH-"}}, 1.10e10, Unit::SecondOrder, 'C', false},
      {6, {{"e-aq"}, {"HO2-"}}, {{"O-"}, {"OH-"}}, 3.50e9, Unit::SecondOrder, 'A', false},
      {7, {{"e-aq"}, {"O2"}}, {{"O2-"}}, 1.90e10, Unit::SecondOrder, 'C', false},
      {8, {{"e-aq"}, {"O2-"}, {"H2O"}}, {{"HO2-"}, {"OH-"}}, 1.30e10, Unit::SecondOrder, 'A', false},
      {9, {{"e-aq"}, {"HO2."}}, {{"HO2-"}}, 1.30e10, Unit::SecondOrder, 'A', false},
      {10, {{"e-aq"}, {"O3-"}, {"H2O"}}, {{"O2"}, {"OH-", 2}}, 1.60e10, Unit::SecondOrder, 'A', false},
      {11, {{"e-aq"}, {"O3"}}, {{"O3-"}}, 3.60e10, Unit::SecondOrder, 'A', false},
      {12, {{"OH."}, {"H2O2"}}, {{"HO2."}, {"H2O"}}, 2.20e7, Unit::SecondOrder, 'D', false},
      {13, {{"OH."}, {"O-"}}, {{"HO2-"}}, 2.50e10, Unit::SecondOrder, 'A', false},
      {14, {{"OH."}, {"HO2-"}}, {{"HO2."}, {"OH-"}}, 7.50e9, Unit::SecondOrder, 'C', false},
      {15, {{"OH."}, {"O3-"}}, {{"O3"}, {"OH-"}}, 2.60e9, Unit::SecondOrder, 'A', false},
      {16, {{"OH."}, {"O3-"}, {"H2O"}}, {{"O2-", 2}, {"H3O+"}}, 6.00e9, Unit::SecondOrder, 'A', false},
      {17, {{"OH."}, {"O3"}}, {{"HO2."}, {"O2"}}, 1.10e8, Unit::SecondOrder, 'A', false},
      {18, {{"OH."}, {"H."}}, {{"H2O"}}, 1.09e10, Unit::SecondOrder, 'A', false},
      {19, {{"OH.", 2}}, {{"H2O2"}}, 5.50e9, Unit::SecondOrder, 'C', false},
      {20, {{"OH."}, {"H2"}}, {{"H."}, {"H2O"}}, 3.95e7, Unit::SecondOrder, 'A', false},
      {21, {{"OH."}, {"HO2."}}, {{"O2"}, {"H2O"}}, 8.84e9, Unit::SecondOrder, 'A', false},
      {22, {{"OH."}, {"O2-"}}, {{"OH-"}, {"O2"}}, 8.00e9, Unit::SecondOrder, 'B', false},
      {23, {{"H."}, {"H2O"}}, {{"H2"}, {"OH."}}, 11.0, Unit::FirstOrder, 'B', true},
      {24, {{"H.", 2}}, {{"H2"}}, 7.52e9, Unit::SecondOrder, 'B', false},
      {25, {{"H."}, {"H2O2"}}, {{"OH."}, {"H2O"}}, 9.00e7, Unit::SecondOrder, 'B', false},
      {26, {{"H."}, {"O-"}}, {{"OH-"}}, 1.00e10, Unit::SecondOrder, 'A', false},
      {27, {{"H."}, {"HO2-"}}, {{"OH."}, {"OH-"}}, 9.00e7, Unit::SecondOrder, 'A', false},
      {28, {{"H."}, {"O3-"}}, {{"OH-"}, {"O2"}}, 1.00e10, Unit::SecondOrder, 'A', false},
      {29, {{"H."}, {"O2-"}}, {{"HO2-"}}, 1.14e10, Unit::SecondOrder, 'A', false},
      {30, {{"H."}, {"O3"}}, {{"HO3"}}, 3.80e10, Unit::SecondOrder, 'A', false},
      {31, {{"H."}, {"O2"}}, {{"HO2."}}, 1.31e10, Unit::SecondOrder, 'A', false},
      {32, {{"H."}, {"HO2."}}, {{"H2O2"}}, 1.14e10, Unit::SecondOrder, 'A', false},
      {33, {{"HO2."}, {"O2-"}}, {{"HO2-"}, {"O2"}}, 8.00e7, Unit::SecondOrder, 'A', false},
      {34, {{"HO2.", 2}}, {{"H2O2"}, {"O2"}}, 8.40e5, Unit::SecondOrder, 'A', false},
      {35, {{"HO2."}, {"O-"}}, {{"O2"}, {"OH-"}}, 6.00e9, Unit::SecondOrder, 'A', false},
      {36, {{"HO2."}, {"H2O2"}}, {{"OH."}, {"O2"}, {"H2O"}}, 0.50, Unit::SecondOrder, 'A', false},
      {37, {{"HO2."}, {"HO2-"}}, {{"OH."}, {"O2"}, {"OH-"}}, 0.50, Unit::SecondOrder, 'A', false},
      {38, {{"HO2."}, {"O3-"}}, {{"O2", 2}, {"OH-"}}, 6.00e9, Unit::SecondOrder, 'A', false},
      {39, {{"HO2."}, {"O3"}}, {{"HO3"}, {"O2"}}, 5.00e8, Unit::SecondOrder, 'A', false},
      {40, {{"O2-"}, {"O-"}, {"H2O"}}, {{"O2"}, {"OH-", 2}}, 6.00e8, Unit::SecondOrder, 'A', false},
      {41, {{"O2-", 2}, {"H2O", 2}}, {{"H2O2"}, {"O2"}, {"OH-", 2}}, 0.3, Unit::SecondOrder, 'A', false},
      {42, {{"O2-"}, {"H2O2"}}, {{"OH."}, {"O2"}, {"OH-"}}, 0.13, Unit::SecondOrder, 'A', false},
      {43, {{"O2-"}, {"HO2-"}}, {{"O-"}, {"O2"}, {"OH-"}}, 0.13, Unit::SecondOrder, 'A', false},
      {44, {{"O2-"}, {"O3-"}, {"H2O"}}, {{"O2", 2}, {"OH-", 2}}, 1.00e4, Unit::SecondOrder, 'A', false},
      {45, {{"O2-"}, {"O3"}}, {{"O3-"}, {"O2"}}, 1.50e9, Unit::SecondOrder, 'A', false},
      {46, {{"O-", 2}, {"H2O"}}, {{"HO2-"}, {"OH-"}}, 1.00e9, Unit::SecondOrder, 'A', false},
      {47, {{"O-"}, {"O2"}}, {{"O3-"}}, 3.75e9, Unit::SecondOrder, 'A', false},
      {48, {{"O-"}, {"H2"}}, {{"H."}, {"OH-"}}, 1.28e8, Unit::SecondOrder, 'A', false},
      {49, {{"O-"}, {"H2O2"}}, {{"O2-"}, {"H2O"}}, 5.00e8, Unit::SecondOrder, 'A', false},
      {50, {{"O-"}, {"HO2-"}}, {{"O2-"}, {"OH-"}}, 7.86e8, Unit::SecondOrder, 'A', false},
      {51, {{"O-"}, {"O3-"}}, {{"O2-", 2}}, 7.00e8, Unit::SecondOrder, 'A', false},
      {52, {{"O-"}, {"O3"}}, {{"O2-"}, {"O2"}}, 5.00e9, Unit::SecondOrder, 'A', false},
      {53, {{"O3-"}}, {{"O2"}, {"O-"}}, 2.62e3, Unit::FirstOrder, 'A', false},
      {54, {{"O3-"}, {"H3O+"}}, {{"O2"}, {"OH."}, {"H2O"}}, 9.00e10, Unit::SecondOrder, 'A', false},
      {55, {{"HO3"}}, {{"O2"}, {"OH."}}, 1.10e5, Unit::FirstOrder, 'A', false},
      {56, {{"HO2."}, {"OH-"}}, {{"O2-"}, {"H2O"}}, 1.33e10, Unit::SecondOrder, 'A', false},
      {57, {{"O2-"}, {"H2O"}}, {{"HO2."}, {"OH-"}}, 0.155, Unit::FirstOrder, 'A', false},
      {58, {{"H2O", 2}}, {{"H3O+"}, {"OH-"}}, 2.11e-5, Unit::ZeroOrder, 'A', true},
      {59, {{"H3O+"}, {"OH-"}}, {{"H2O", 2}}, 1.18e11, Unit::SecondOrder, 'A', true},
      {60, {{"OH."}, {"H2O"}}, {{"H3O+"}, {"O-"}}, 9.43e-2, Unit::FirstOrder, 'A', true},
      {61, {{"H3O+"}, {"O-"}}, {{"OH."}, {"H2O"}}, 5.02e10, Unit::SecondOrder, 'A', true},
      {62, {{"H2O2"}, {"H2O"}}, {{"H3O+"}, {"HO2-"}}, 9.43e-2, Unit::FirstOrder, 'A', true},
      {63, {{"H3O+"}, {"HO2-"}}, {{"H2O2"}, {"H2O"}}, 5.02e10, Unit::SecondOrder, 'A', true},
      {64, {{"HO2."}, {"H2O"}}, {{"H3O+"}, {"O2-"}}, 7.73e5, Unit::FirstOrder, 'A', true},
      {65, {{"H3O+"}, {"O2-"}}, {{"HO2."}, {"H2O"}}, 5.02e10, Unit::SecondOrder, 'A', true},
      {66, {{"e-aq"}, {"H2O"}}, {{"H."}, {"OH-"}}, 19.0, Unit::FirstOrder, 'D', true},
      {67, {{"H."}, {"OH-"}}, {{"e-aq"}, {"H2O"}}, 2.20e7, Unit::SecondOrder, 'D', true},
      {68, {{"H2O2"}, {"OH-"}}, {{"HO2-"}, {"H2O"}}, 1.33e10, Unit::SecondOrder, 'A', true},
      {69, {{"HO2-"}, {"H2O"}}, {{"H2O2"}, {"OH-"}}, 1.27e6, Unit::FirstOrder, 'A', true},
      {70, {{"e-aq"}, {"H3O+"}}, {{"H."}, {"H2O"}}, 2.09e10, Unit::SecondOrder, 'A', true},
      {71, {{"H."}, {"H2O"}}, {{"e-aq"}, {"H3O+"}}, 5.83, Unit::FirstOrder, 'A', true},
      {72, {{"OH."}, {"OH-"}}, {{"O-"}, {"H2O"}}, 1.33e10, Unit::SecondOrder, 'A', true},
      {73, {{"O-"}, {"H2O"}}, {{"OH."}, {"OH-"}}, 1.27e6, Unit::FirstOrder, 'A', true},
  };
}
}  // namespace

const std::vector<Reaction>& Reactions()
{
  static const std::vector<Reaction> table = MakeReactions();
  return table;
}

const Reaction* Find(int id)
{
  const auto& table = Reactions();
  if (id < 1 || id > static_cast<int>(table.size())) {
    return nullptr;
  }
  return &table[static_cast<std::size_t>(id - 1)];
}

const char* UnitLabel(Unit unit)
{
  switch (unit) {
    case Unit::SecondOrder:
      return "M^-1 s^-1";
    case Unit::FirstOrder:
      return "s^-1";
    case Unit::ZeroOrder:
      return "M s^-1";
  }
  return "";
}
}  // namespace Tonneau2025Table
