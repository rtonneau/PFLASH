#include "PrecursorPacket.hh"

#include <cmath>
#include <limits>
#include <sstream>

#include "TestCheck.hh"

using namespace ptirt;

namespace
{

PrecursorPacket MakePacket()
{
  PrecursorPacket p;
  p.compatibilityTag = "test-tag-1";
  p.primaryParticle = "e-";
  p.primaryEnergyEv = 10000.0;
  p.energyDepositEv = 9990.123456789;
  p.escapedEnergyEv = 10000.0 - 9990.123456789;
  p.captureHalfSideNm = 5000.0;
  p.captureTimeNs = 0.001;
  for (int i = 0; i < 5; ++i)
  {
    PacketMolecule m;
    m.definitionName = i % 2 ? "OH" : "e_aq";
    m.charge = i % 2 ? 0 : -1;
    m.status = i % 2;
    m.electronOccupancy = {2, 2, 2, 1, 2};
    m.positionNm = {0.1 * i + 1.0 / 3.0, -2.5e-3 * i, 1e-300 * i};
    m.direction = {0.6, 0.0, 0.8};
    m.globalTimeNs = 0.001;
    m.localTimeNs = 0.0;
    m.properTimeNs = 0.0;
    m.decayTimeNs = -1.0;  // Geant4 stable sentinel
    m.kineticEnergyEv = 0.025 * i;
    p.molecules.push_back(m);
  }
  return p;
}

std::string Serialize(const PrecursorPacket& p)
{
  std::ostringstream out;
  Write(out, p);
  return out.str();
}

PrecursorPacket Parse(const std::string& text, const std::string& tag = "test-tag-1")
{
  std::istringstream in(text);
  return Read(in, tag);
}

void ReplaceFirst(std::string& text, const std::string& from, const std::string& to)
{
  const auto pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), to);
}

void TestRoundTrip()
{
  const PrecursorPacket p = MakePacket();
  const std::string text = Serialize(p);
  const PrecursorPacket q = Parse(text);
  CHECK(p == q);  // exact doubles, bitwise via ==
  CHECK(Serialize(q) == text);

  PrecursorPacket empty = MakePacket();
  empty.molecules.clear();
  CHECK(Parse(Serialize(empty)) == empty);
}

void TestValidateRejects()
{
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  {
    PrecursorPacket p = MakePacket();
    p.energyDepositEv = nan;
    CHECK_THROWS(Validate(p), "non-finite");
  }
  {
    PrecursorPacket p = MakePacket();
    p.captureHalfSideNm = inf;
    CHECK_THROWS(Validate(p), "non-finite");
  }
  {
    PrecursorPacket p = MakePacket();
    p.energyDepositEv = -1.0;
    CHECK_THROWS(Validate(p), "negative energy deposit");
  }
  {
    PrecursorPacket p = MakePacket();
    p.escapedEnergyEv = -1.0;
    CHECK_THROWS(Validate(p), "negative escaped");
  }
  {
    PrecursorPacket p = MakePacket();
    p.primaryEnergyEv = -5.0;
    CHECK_THROWS(Validate(p), "primary energy");
  }
  {
    PrecursorPacket p = MakePacket();
    p.energyDepositEv = p.primaryEnergyEv + 1.0;
    p.escapedEnergyEv = 0;
    CHECK_THROWS(Validate(p), "exceeds the primary");
  }
  {
    PrecursorPacket p = MakePacket();
    p.escapedEnergyEv += 5.0;  // deposit + escaped > primary
    CHECK_THROWS(Validate(p), "does not match");
  }
  {
    PrecursorPacket p = MakePacket();
    p.escapedEnergyEv = 0;  // energy lost: deposit + escaped < primary
    CHECK_THROWS(Validate(p), "does not match");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[1].electronOccupancy[0] = -1;
    CHECK_THROWS(Validate(p), "occupancy");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[1].electronOccupancy[0] = 99;
    CHECK_THROWS(Validate(p), "occupancy");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[2].direction = {1.0, 1.0, 0.0};
    CHECK_THROWS(Validate(p), "unit vector");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[2].positionNm[1] = nan;
    CHECK_THROWS(Validate(p), "position");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[0].kineticEnergyEv = -0.1;
    CHECK_THROWS(Validate(p), "negative");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[0].status = 7;
    CHECK_THROWS(Validate(p), "status");
  }
  {
    PrecursorPacket p = MakePacket();
    p.molecules[0].definitionName = "bad name";
    CHECK_THROWS(Validate(p), "definition name");
  }
  {
    PrecursorPacket p = MakePacket();
    p.compatibilityTag.clear();
    CHECK_THROWS(Validate(p), "compatibility tag");
  }
  {
    PacketLimits limits;
    limits.maxMolecules = 2;
    CHECK_THROWS(Validate(MakePacket(), limits), "too many molecules");
  }
  // Write refuses invalid packets.
  {
    PrecursorPacket p = MakePacket();
    p.energyDepositEv = nan;
    std::ostringstream out;
    CHECK_THROWS(Write(out, p), "non-finite");
  }
}

void TestReadRejects()
{
  const std::string good = Serialize(MakePacket());

  CHECK_THROWS(Parse(good, "other-tag"), "incompatible");
  {
    std::string t = good;
    ReplaceFirst(t, "PTIRT-PRECURSOR-PACKET", "SOMETHING-ELSE");
    CHECK_THROWS(Parse(t), "wrong magic");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "PACKET 1", "PACKET 2");
    CHECK_THROWS(Parse(t), "unsupported version");
  }
  CHECK_THROWS(Parse(""), "truncated");
  // Truncation at every line boundary and in the middle of the text.
  for (std::size_t cut = 1; cut < good.size(); cut += 7)
  {
    bool rejected = false;
    try { Parse(good.substr(0, cut)); }
    catch (const std::exception&) { rejected = true; }
    // A cut that only removes the final newline leaves a complete packet.
    if (cut + 1 < good.size()) CHECK(rejected);
  }
  CHECK_THROWS(Parse(good + "garbage\n"), "trailing");
  CHECK_THROWS(Parse(good + "end\n"), "trailing");
  {
    std::string t = good;
    ReplaceFirst(t, "moleculeCount 5", "moleculeCount 6");
    CHECK_THROWS(Parse(t), "");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "moleculeCount 5", "moleculeCount -1");
    CHECK_THROWS(Parse(t), "negative");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "energyDeposit_eV 0x", "energyDeposit_eV nan 0x");
    CHECK_THROWS(Parse(t), "");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "captureTime_ns ", "captureTime_ns nan #");
    CHECK_THROWS(Parse(t), "");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "captureTime_ns 0x", "captureTime_ns inf");
    // remainder of the old number now parses as a stray token -> rejected either way
    CHECK_THROWS(Parse(t), "");
  }
  {
    std::string t = good;
    ReplaceFirst(t, "primaryEnergy_eV", "primaryEnergy");
    CHECK_THROWS(Parse(t), "expected");
  }
}

void TestTranslate()
{
  const PrecursorPacket p = MakePacket();
  const PrecursorPacket q = Translate(p, {100.0, -50.0, 25.0});
  CHECK(q.molecules.size() == p.molecules.size());
  for (std::size_t i = 0; i < p.molecules.size(); ++i)
  {
    CHECK(q.molecules[i].positionNm[0] == p.molecules[i].positionNm[0] + 100.0);
    CHECK(q.molecules[i].positionNm[1] == p.molecules[i].positionNm[1] - 50.0);
    CHECK(q.molecules[i].globalTimeNs == p.molecules[i].globalTimeNs);
    CHECK(q.molecules[i].direction == p.molecules[i].direction);
  }
  CHECK_THROWS(Translate(p, {std::numeric_limits<double>::quiet_NaN(), 0, 0}), "non-finite");
}

}  // namespace

int main()
{
  TestRoundTrip();
  TestValidateRejects();
  TestReadRejects();
  TestTranslate();
  std::puts("PrecursorPacketTest passed");
  return 0;
}
