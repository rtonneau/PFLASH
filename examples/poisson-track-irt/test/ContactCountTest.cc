#include <cmath>
#include <string>
#include <vector>

#include "ContactCount.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

// 1.0 for A + B (either order), no reaction for anything else.
double RadiusAB(const std::string& a, const std::string& b)
{
  return (a == "A" && b == "B") || (a == "B" && b == "A") ? 1.0 : -1.0;
}

ContactMolecule Mol(const std::string& species, double x, double y = 0, double z = 0)
{
  return ContactMolecule{species, {x, y, z}};
}

void TestPairLabel()
{
  CHECK(PairLabel("B", "A") == "A + B");
  CHECK(PairLabel("A", "B") == "A + B");
  CHECK(PairLabel("A", "A") == "A + A");
}

void TestNewOld()
{
  auto counts = CountNewOldContacts({Mol("A", 0)}, {Mol("B", 0.999)}, RadiusAB);
  CHECK(counts.pairs == 1);
  CHECK(counts.newMoleculesInContact == 1);
  CHECK(counts.perPair.size() == 1);
  CHECK(counts.perPair.at("A + B") == 1);
  CHECK(counts.cpuSeconds >= 0);

  // Strict: a distance equal to the radius is not a contact.
  counts = CountNewOldContacts({Mol("A", 0)}, {Mol("B", 1.0)}, RadiusAB);
  CHECK(counts.pairs == 0);
  CHECK(counts.newMoleculesInContact == 0);
  CHECK(counts.perPair.empty());

  // The distance is three-dimensional: 0.6, 0.6, 0.6 is 1.039 away.
  CHECK(CountNewOldContacts({Mol("A", 0, 0, 0)}, {Mol("B", 0.6, 0.6, 0.6)}, RadiusAB).pairs == 0);
  CHECK(CountNewOldContacts({Mol("A", 0, 0, 0)}, {Mol("B", 0.5, 0.5, 0.5)}, RadiusAB).pairs == 1);

  // Species that do not react (radius < 0) never count, however close.
  counts = CountNewOldContacts({Mol("A", 0), Mol("A", 0.1)}, {Mol("A", 0.2), Mol("A", 0)}, RadiusAB);
  CHECK(counts.pairs == 0);

  // A radius of 0 gives no contact at distance 0.
  CHECK(CountNewOldContacts({Mol("A", 0)}, {Mol("B", 0)}, [](const std::string&, const std::string&) { return 0.0; })
            .pairs == 0);

  // Two fresh molecules near one old one: two pairs, two fresh in contact.
  counts = CountNewOldContacts({Mol("A", 0), Mol("A", 0.5)}, {Mol("B", 0.2)}, RadiusAB);
  CHECK(counts.pairs == 2);
  CHECK(counts.newMoleculesInContact == 2);

  // One fresh molecule near two old ones: two pairs, one fresh in contact.
  counts = CountNewOldContacts({Mol("A", 0)}, {Mol("B", 0.2), Mol("B", -0.3)}, RadiusAB);
  CHECK(counts.pairs == 2);
  CHECK(counts.newMoleculesInContact == 1);
  CHECK(counts.perPair.at("A + B") == 2);

  // The pair label does not depend on which side is fresh.
  counts = CountNewOldContacts({Mol("B", 0)}, {Mol("A", 0.5)}, RadiusAB);
  CHECK(counts.pairs == 1);
  CHECK(counts.perPair.at("A + B") == 1);

  // Empty sides.
  CHECK(CountNewOldContacts({}, {Mol("B", 0)}, RadiusAB).pairs == 0);
  CHECK(CountNewOldContacts({Mol("A", 0)}, {}, RadiusAB).pairs == 0);
}

// The sorted-window search must agree with the plain double loop.
void TestNewOldMatchesBruteForce()
{
  std::vector<ContactMolecule> fresh;
  std::vector<ContactMolecule> old;
  unsigned state = 12345;
  const auto next = [&state] {
    state = state * 1664525u + 1013904223u;
    return static_cast<double>(state >> 8) / static_cast<double>(1u << 24) * 6.0;
  };
  for (int i = 0; i < 300; ++i)
  {
    fresh.push_back(Mol(i % 2 == 0 ? "A" : "B", next(), next(), next()));
    old.push_back(Mol(i % 3 == 0 ? "A" : "B", next(), next(), next()));
  }
  std::size_t pairs = 0;
  for (const auto& f : fresh)
    for (const auto& o : old)
    {
      const double r = RadiusAB(f.species, o.species);
      if (r <= 0) continue;
      const double dx = f.positionNm[0] - o.positionNm[0];
      const double dy = f.positionNm[1] - o.positionNm[1];
      const double dz = f.positionNm[2] - o.positionNm[2];
      if (dx * dx + dy * dy + dz * dz < r * r) ++pairs;
    }
  CHECK(pairs > 0);
  CHECK(CountNewOldContacts(fresh, old, RadiusAB).pairs == pairs);
  std::vector<ContactMolecule> all = fresh;
  all.insert(all.end(), old.begin(), old.end());
  std::size_t internal = 0;
  for (std::size_t i = 0; i < all.size(); ++i)
    for (std::size_t j = i + 1; j < all.size(); ++j)
    {
      const double r = RadiusAB(all[i].species, all[j].species);
      if (r <= 0) continue;
      const double dx = all[i].positionNm[0] - all[j].positionNm[0];
      const double dy = all[i].positionNm[1] - all[j].positionNm[1];
      const double dz = all[i].positionNm[2] - all[j].positionNm[2];
      if (dx * dx + dy * dy + dz * dz < r * r) ++internal;
    }
  CHECK(internal > 0);
  CHECK(CountInternalContacts(all, RadiusAB).pairs == internal);
}

void TestInternal()
{
  // Each unordered pair once, whatever the input order.
  auto counts = CountInternalContacts({Mol("A", 0), Mol("B", 0.5)}, RadiusAB);
  CHECK(counts.pairs == 1);
  CHECK(counts.newMoleculesInContact == 2);
  CHECK(counts.perPair.at("A + B") == 1);
  CHECK(CountInternalContacts({Mol("B", 0.5), Mol("A", 0)}, RadiusAB).pairs == 1);

  // Strict.
  CHECK(CountInternalContacts({Mol("A", 0), Mol("B", 1.0)}, RadiusAB).pairs == 0);

  // A chain A - B - A: two pairs, three molecules in contact.
  counts = CountInternalContacts({Mol("A", 0), Mol("B", 0.6), Mol("A", 1.2)}, RadiusAB);
  CHECK(counts.pairs == 2);
  CHECK(counts.newMoleculesInContact == 3);
  CHECK(counts.perPair.at("A + B") == 2);

  // Like species do not pair; a single molecule has no partner.
  CHECK(CountInternalContacts({Mol("A", 0), Mol("A", 0.1), Mol("A", 0.2)}, RadiusAB).pairs == 0);
  CHECK(CountInternalContacts({Mol("A", 0)}, RadiusAB).pairs == 0);
  CHECK(CountInternalContacts({}, RadiusAB).pairs == 0);

  // Coincident molecules are in contact when the radius is positive.
  CHECK(CountInternalContacts({Mol("A", 2, 2, 2), Mol("B", 2, 2, 2)}, RadiusAB).pairs == 1);
}

}  // namespace

int main()
{
  TestPairLabel();
  TestNewOld();
  TestNewOldMatchesBruteForce();
  TestInternal();
  return 0;
}
