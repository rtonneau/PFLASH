#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "CoincidenceCheck.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

using Positions = std::vector<std::array<double, 3>>;

CoincidenceMolecule Mol(double x, double y, double z, std::size_t packet = 0, int group = 0)
{
  CoincidenceMolecule m;
  m.position = {x, y, z};
  m.packet = packet;
  m.productGroup = group;
  return m;
}

void TestCounts()
{
  auto report = FindCoincidentPositions(Positions{});
  CHECK(report.coincidentMolecules == 0 && report.positions == 0);

  report = FindCoincidentPositions(Positions{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
  CHECK(report.coincidentMolecules == 0 && report.positions == 0);

  // Two equal and one distinct: 2 molecules, 1 position.
  report = FindCoincidentPositions(Positions{{1, 2, 3}, {5, 5, 5}, {1, 2, 3}});
  CHECK(report.coincidentMolecules == 2 && report.positions == 1);

  // Three equal and two equal: 5 molecules, 2 positions.
  report = FindCoincidentPositions(Positions{{1, 1, 1}, {2, 2, 2}, {1, 1, 1}, {2, 2, 2}, {1, 1, 1}});
  CHECK(report.coincidentMolecules == 5 && report.positions == 2);

  // Positions only: nothing has a product group, so nothing is exempt.
  CHECK(report.exemptMolecules == 0 && report.exemptPositions == 0);
  CHECK(report.forbiddenMolecules == 5 && report.forbiddenPositions == 2);
}

void TestExactEquality()
{
  const double x = 1.0;
  const double next = std::nextafter(x, 2.0);
  CHECK(next != x);
  for (int axis = 0; axis < 3; ++axis)
  {
    std::array<double, 3> a{3, 4, 5};
    auto b = a;
    b[static_cast<std::size_t>(axis)] = std::nextafter(a[static_cast<std::size_t>(axis)], 10.0);
    const auto report = FindCoincidentPositions(Positions{a, b});
    CHECK(report.coincidentMolecules == 0 && report.positions == 0);
  }
  // Signed zeros compare equal.
  CHECK(FindCoincidentPositions(Positions{{0.0, 0.0, 0.0}, {-0.0, 0.0, -0.0}}).coincidentMolecules == 2);
  // Equal in two coordinates is not enough.
  CHECK(FindCoincidentPositions(Positions{{1, 2, 3}, {1, 2, 4}, {1, 9, 3}, {7, 2, 3}}).coincidentMolecules == 0);
}

void TestNonFinite()
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS(FindCoincidentPositions(Positions{{0, 0, 0}, {nan, 0, 0}}), "not finite");
  CHECK_THROWS(FindCoincidentPositions(Positions{{0, 0, std::numeric_limits<double>::infinity()}}), "not finite");
}

void TestClassification()
{
  // Products of one start-up reaction at one point: allowed.
  auto report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{
      Mol(1, 1, 1, 0, 2), Mol(9, 9, 9, 0, 0), Mol(1, 1, 1, 0, 2), Mol(1, 1, 1, 0, 2)});
  CHECK(report.coincidentMolecules == 3 && report.positions == 1);
  CHECK(report.exemptMolecules == 3 && report.exemptPositions == 1);
  CHECK(report.forbiddenMolecules == 0 && report.forbiddenPositions == 0);
  CHECK(report.exemptMembers.size() == 1);
  CHECK((report.exemptMembers[0] == std::vector<std::size_t>{0, 2, 3}));

  // Two precursors (group 0): refused.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{Mol(1, 1, 1), Mol(1, 1, 1)});
  CHECK(report.exemptPositions == 0 && report.forbiddenMolecules == 2 && report.forbiddenPositions == 1);
  CHECK(report.exemptMembers.empty());

  // A precursor and a product: refused.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{Mol(1, 1, 1, 0, 0), Mol(1, 1, 1, 0, 1)});
  CHECK(report.forbiddenMolecules == 2 && report.exemptMolecules == 0);

  // Products of two reactions at one point: refused.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{Mol(1, 1, 1, 0, 1), Mol(1, 1, 1, 0, 2)});
  CHECK(report.forbiddenMolecules == 2 && report.exemptMolecules == 0);

  // The same group number in two packets is two different reactions: refused.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{Mol(1, 1, 1, 0, 1), Mol(1, 1, 1, 1, 1)});
  CHECK(report.forbiddenMolecules == 2 && report.forbiddenPositions == 1 && report.exemptMolecules == 0);

  // Mixed set: one exempt position, one refused position.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{
      Mol(1, 1, 1, 0, 1), Mol(1, 1, 1, 0, 1), Mol(2, 2, 2), Mol(2, 2, 2), Mol(2, 2, 2), Mol(3, 3, 3, 0, 1)});
  CHECK(report.coincidentMolecules == 5 && report.positions == 2);
  CHECK(report.exemptMolecules == 2 && report.exemptPositions == 1);
  CHECK(report.forbiddenMolecules == 3 && report.forbiddenPositions == 1);

  // A lone product (nothing else at its point) is no coincidence.
  report = FindCoincidentPositions(std::vector<CoincidenceMolecule>{Mol(1, 1, 1, 0, 1)});
  CHECK(report.coincidentMolecules == 0);
}

void TestMessage()
{
  const auto report = FindCoincidentPositions(Positions{{1, 1, 1}, {1, 1, 1}, {2, 2, 2}, {2, 2, 2}, {2, 2, 2}});
  CHECK(DescribeForbiddenCoincidences(report) == "PTIRT_COINCIDENT_MOLECULES count=5 positions=2");
  CHECK(DescribeForbiddenCoincidences(CoincidenceReport{}) == "PTIRT_COINCIDENT_MOLECULES count=0 positions=0");
}

double RadiusOhH2(const std::string& a, const std::string& b)
{
  // Only OH + OH reacts.
  return a == "OH" && b == "OH" ? 0.5 : -1.0;
}

void TestSummary()
{
  // Group 1: OH, OH, H2 at one point -> 1 reactive pair (OH + OH).
  // Group 2: H2, H2 at another -> none.
  const std::vector<CoincidenceMolecule> molecules{Mol(1, 1, 1, 0, 1), Mol(2, 2, 2, 0, 2), Mol(1, 1, 1, 0, 1),
                                                   Mol(1, 1, 1, 0, 1), Mol(2, 2, 2, 0, 2), Mol(8, 8, 8)};
  const std::vector<std::string> species{"OH", "H2", "OH", "H2", "H2", "OH"};
  const auto report = FindCoincidentPositions(molecules);
  CHECK(report.exemptPositions == 2 && report.exemptMolecules == 5 && report.forbiddenMolecules == 0);

  const auto summary = SummarizeExempt(report, species, RadiusOhH2);
  CHECK(summary.exemptPositions == 2 && summary.exemptMolecules == 5);
  CHECK(summary.reactivePairs == 1);
  CHECK(summary.perPair.size() == 1 && summary.perPair.at("OH + OH") == 1);

  CoincidenceSummary total;
  total.Add(summary);
  total.Add(summary);
  CHECK(total.exemptPositions == 4 && total.exemptMolecules == 10 && total.reactivePairs == 2);
  CHECK(total.perPair.at("OH + OH") == 2);

  CHECK_THROWS(SummarizeExempt(report, std::vector<std::string>{"OH"}, RadiusOhH2), "no species");
}

}  // namespace

int main()
{
  TestCounts();
  TestExactEquality();
  TestNonFinite();
  TestClassification();
  TestMessage();
  TestSummary();
  return 0;
}
