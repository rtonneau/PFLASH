#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "HistoryConfig.hh"
#include "PacketDigest.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

PacketMolecule Molecule(const std::string& name, double x, double y, double z, double timeNs = 0.001)
{
  PacketMolecule m;
  m.definitionName = name;
  m.positionNm = {x, y, z};
  m.globalTimeNs = timeNs;
  return m;
}

std::vector<PacketMolecule> Sample()
{
  return {Molecule("OH", 1.0, 2.0, 3.0), Molecule("e_aq", -4.5, 0.25, 7.0),
          Molecule("H3O", 1.0, 2.0, 3.0), Molecule("OH", 1.0, 2.0, 3.0000001),
          Molecule("OH", 0.0, 0.0, 0.0)};
}

void TestDigestIsOrderIndependent()
{
  const auto molecules = Sample();
  const auto digest = DigestMolecules(molecules);

  auto reversed = molecules;
  std::reverse(reversed.begin(), reversed.end());
  CHECK(DigestMolecules(reversed) == digest);

  // Every rotation and a few swaps.
  auto rotated = molecules;
  for (std::size_t i = 0; i < rotated.size(); ++i)
  {
    std::rotate(rotated.begin(), rotated.begin() + 1, rotated.end());
    CHECK(DigestMolecules(rotated) == digest);
  }
  auto swapped = molecules;
  std::swap(swapped[0], swapped[3]);
  std::swap(swapped[1], swapped[4]);
  CHECK(DigestMolecules(swapped) == digest);

  // The digest reads name, position and time only: the other fields (and the
  // product group) are not part of it.
  auto otherFields = molecules;
  for (auto& m : otherFields)
  {
    m.productGroup = 3;
    m.direction = {0, 1, 0};
    m.kineticEnergyEv = 5;
    m.charge = -1;
  }
  CHECK(DigestMolecules(otherFields) == digest);
}

void TestDigestSeesEachInput()
{
  const auto molecules = Sample();
  const auto digest = DigestMolecules(molecules);

  // One unit in the last place of x.
  auto nudged = molecules;
  nudged[1].positionNm[0] = std::nextafter(nudged[1].positionNm[0], 1e300);
  CHECK(nudged[1].positionNm[0] != molecules[1].positionNm[0]);
  CHECK(DigestMolecules(nudged) != digest);

  for (int axis = 1; axis < 3; ++axis)
  {
    auto other = molecules;
    other[2].positionNm[axis] = std::nextafter(other[2].positionNm[axis], -1e300);
    CHECK(DigestMolecules(other) != digest);
  }

  auto later = molecules;
  later[0].globalTimeNs = std::nextafter(later[0].globalTimeNs, 1e300);
  CHECK(DigestMolecules(later) != digest);

  auto renamed = molecules;
  renamed[1].definitionName = "e_aq2";
  CHECK(DigestMolecules(renamed) != digest);
  renamed[1].definitionName = "E_aq";
  CHECK(DigestMolecules(renamed) != digest);

  // +0 and -0 are different states of the position bits.
  auto negativeZero = molecules;
  negativeZero[4].positionNm[0] = -0.0;
  CHECK(DigestMolecules(negativeZero) != digest);

  // A molecule more, a molecule less, a duplicate.
  auto fewer = molecules;
  fewer.pop_back();
  CHECK(DigestMolecules(fewer) != digest);
  auto more = molecules;
  more.push_back(molecules[0]);
  CHECK(DigestMolecules(more) != digest);
  CHECK(DigestMolecules(more) != DigestMolecules(fewer));

  // The name and the numbers do not run into each other.
  CHECK(DigestMolecules({Molecule("A", 1, 2, 3)}) != DigestMolecules({Molecule("AB", 1, 2, 3)}));
}

void TestDigestValues()
{
  // FNV-1a 64 (offset 0xcbf29ce484222325, prime 0x100000001b3) of the 8 zero bytes of the empty list.
  CHECK(DigestMolecules({}) == 0xa8c7f832281a39c5ULL);
  CHECK(DigestMolecules({}) == DigestMolecules({}));
  CHECK(DigestMolecules(Sample()) == DigestMolecules(Sample()));
  CHECK(DigestMolecules(Sample()) != DigestMolecules({}));
}

void TestDigestHex()
{
  CHECK(DigestToHex(0) == "0000000000000000");
  CHECK(DigestToHex(0xaf63bd4c8601b7dfULL) == "af63bd4c8601b7df");
  CHECK(DigestToHex(~0ULL) == "ffffffffffffffff");
  CHECK(DigestToHex(DigestMolecules(Sample())).size() == 16);
}

void TestCanonicalOrder()
{
  auto molecules = Sample();
  std::sort(molecules.begin(), molecules.end(), CanonicalLess);
  CHECK(molecules.front().definitionName == "H3O");  // name first
  for (std::size_t i = 1; i < molecules.size(); ++i) CHECK(!CanonicalLess(molecules[i], molecules[i - 1]));
  CHECK(!CanonicalLess(molecules[0], molecules[0]));
  // Negative before positive, -0 before +0.
  CHECK(CanonicalLess(Molecule("OH", -2, 0, 0), Molecule("OH", -1, 0, 0)));
  CHECK(CanonicalLess(Molecule("OH", -1, 0, 0), Molecule("OH", 0, 0, 0)));
  CHECK(CanonicalLess(Molecule("OH", -0.0, 0, 0), Molecule("OH", 0.0, 0, 0)));
  CHECK(CanonicalLess(Molecule("OH", 1, 0, 0), Molecule("OH", 1, 0, 1)));
  CHECK(CanonicalLess(Molecule("OH", 1, 0, 0, 0.001), Molecule("OH", 1, 0, 0, 0.002)));
}

void TestChemistrySeed()
{
  // The rule of the stock build: kMaxHistorySeed + 1 - physicsSeed.
  CHECK(kMaxStockPhysicsSeed == 1073741823ULL);
  CHECK(ChemistrySeedFor(1) == kMaxHistorySeed);
  CHECK(ChemistrySeedFor(1001) == 2147483648ULL - 1001);
  CHECK(ChemistrySeedFor(kMaxStockPhysicsSeed) == 1073741825ULL);
  // The two ranges never meet and stay inside the 32-bit seed range.
  CHECK(ChemistrySeedFor(kMaxStockPhysicsSeed) > kMaxStockPhysicsSeed);
  for (std::uint64_t seed : {1ULL, 2ULL, 1001ULL, 536870912ULL, kMaxStockPhysicsSeed})
  {
    CHECK(ChemistrySeedFor(seed) >= 1);
    CHECK(ChemistrySeedFor(seed) <= kMaxHistorySeed);
    CHECK(ChemistrySeedFor(seed) != seed);
    CHECK(ChemistrySeedFor(seed) > kMaxStockPhysicsSeed);
  }
  CHECK_THROWS(ChemistrySeedFor(0), "physics seed");
  CHECK_THROWS(ChemistrySeedFor(kMaxStockPhysicsSeed + 1), "1073741823");
  CHECK_THROWS(ChemistrySeedFor(kMaxHistorySeed), "physics seed");
}

}  // namespace

int main()
{
  TestDigestIsOrderIndependent();
  TestDigestSeesEachInput();
  TestDigestValues();
  TestDigestHex();
  TestCanonicalOrder();
  TestChemistrySeed();
  std::puts("PacketDigestTest passed");
  return 0;
}
