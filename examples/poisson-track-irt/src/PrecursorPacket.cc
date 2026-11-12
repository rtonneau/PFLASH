#include "PrecursorPacket.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace ptirt
{
namespace
{

constexpr const char* kMagic = "PTIRT-PRECURSOR-PACKET";
constexpr int kVersion = 2;

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error("PrecursorPacket: " + message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

bool IsToken(const std::string& s)
{
  if (s.empty() || s.size() > 256) return false;
  for (unsigned char c : s)
    if (c <= ' ' || c == 0x7f) return false;
  return true;
}

// Exact text form of a double (hexadecimal float).
void PutReal(std::ostream& out, double x)
{
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%a", x);
  out << buffer;
}

std::string NextToken(std::istream& in, const std::string& what)
{
  std::string token;
  if (!(in >> token)) Fail("truncated input while reading " + what);
  return token;
}

void ExpectKeyword(std::istream& in, const std::string& keyword)
{
  const std::string token = NextToken(in, "'" + keyword + "'");
  Require(token == keyword, "expected '" + keyword + "' but found '" + token + "'");
}

double GetReal(std::istream& in, const std::string& what)
{
  const std::string token = NextToken(in, what);
  char* end = nullptr;
  const double value = std::strtod(token.c_str(), &end);
  Require(end != token.c_str() && *end == '\0', "malformed number '" + token + "' for " + what);
  return value;
}

long long GetInteger(std::istream& in, const std::string& what)
{
  const std::string token = NextToken(in, what);
  char* end = nullptr;
  const long long value = std::strtoll(token.c_str(), &end, 10);
  Require(end != token.c_str() && *end == '\0', "malformed integer '" + token + "' for " + what);
  return value;
}

std::size_t GetCount(std::istream& in, const std::string& what, std::size_t maximum)
{
  const long long value = GetInteger(in, what);
  Require(value >= 0, what + " is negative");
  Require(static_cast<unsigned long long>(value) <= maximum, what + " exceeds the limit");
  return static_cast<std::size_t>(value);
}

std::string GetFieldToken(std::istream& in, const std::string& keyword)
{
  ExpectKeyword(in, keyword);
  return NextToken(in, keyword);
}

double GetField(std::istream& in, const std::string& keyword)
{
  ExpectKeyword(in, keyword);
  return GetReal(in, keyword);
}

bool AllFinite(const std::array<double, 3>& v)
{
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

}  // namespace

void Validate(const PrecursorPacket& p, const PacketLimits& limits)
{
  Require(IsToken(p.compatibilityTag), "compatibility tag must be a non-empty token without whitespace");
  Require(IsToken(p.primaryParticle), "primary particle must be a non-empty token without whitespace");
  for (double x : {p.primaryEnergyEv, p.energyDepositEv, p.escapedEnergyEv, p.captureHalfSideNm,
                   p.captureTimeNs})
    Require(std::isfinite(x), "non-finite value in packet header");
  Require(p.primaryEnergyEv > 0, "primary energy must be positive");
  Require(p.energyDepositEv >= 0, "negative energy deposit");
  Require(p.escapedEnergyEv >= 0, "negative escaped energy");
  const double tolerance = limits.energyAbsTolEv + limits.energyRelTol * p.primaryEnergyEv;
  Require(p.energyDepositEv <= p.primaryEnergyEv + tolerance,
          "energy deposit exceeds the primary energy");
  Require(std::abs(p.energyDepositEv + p.escapedEnergyEv - p.primaryEnergyEv) <= tolerance,
          "deposit + escaped energy does not match the primary energy within tolerance");
  Require(p.captureHalfSideNm > 0, "capture half side must be positive");
  Require(p.captureTimeNs > 0, "capture time (precursor age) must be positive");
  Require(p.molecules.size() <= limits.maxMolecules, "too many molecules");

  for (const auto& m : p.molecules)
  {
    Require(IsToken(m.definitionName), "molecule definition name must be a non-empty token");
    Require(m.status == 0 || m.status == 1, "molecule status must be 0 or 1");
    Require(m.productGroup >= 0, "molecule product group is negative");
    Require(static_cast<std::size_t>(m.productGroup) <= p.molecules.size(),
            "molecule product group exceeds the molecule count");
    Require(m.electronOccupancy.size() <= limits.maxOccupancyLevels, "too many occupancy levels");
    for (int n : m.electronOccupancy)
      Require(n >= 0 && n <= limits.maxOccupancyPerLevel, "electron occupancy out of range");
    Require(AllFinite(m.positionNm), "non-finite molecule position");
    Require(AllFinite(m.direction), "non-finite molecule direction");
    const double norm = std::sqrt(m.direction[0] * m.direction[0] + m.direction[1] * m.direction[1] +
                                  m.direction[2] * m.direction[2]);
    Require(std::abs(norm - 1.0) <= limits.directionNormTol, "molecule direction is not a unit vector");
    for (double t : {m.globalTimeNs, m.localTimeNs, m.properTimeNs, m.kineticEnergyEv})
      Require(std::isfinite(t) && t >= 0, "molecule time or kinetic energy is negative or non-finite");
    Require(std::isfinite(m.decayTimeNs), "non-finite molecule decay time");
  }
}

void Write(std::ostream& out, const PrecursorPacket& p, const PacketLimits& limits)
{
  Validate(p, limits);
  out << kMagic << ' ' << kVersion << '\n';
  out << "compatibilityTag " << p.compatibilityTag << '\n';
  out << "primaryParticle " << p.primaryParticle << '\n';
  const std::pair<const char*, double> header[] = {
      {"primaryEnergy_eV", p.primaryEnergyEv}, {"energyDeposit_eV", p.energyDepositEv},
      {"escapedEnergy_eV", p.escapedEnergyEv}, {"captureHalfSide_nm", p.captureHalfSideNm},
      {"captureTime_ns", p.captureTimeNs}};
  for (const auto& [name, value] : header)
  {
    out << name << ' ';
    PutReal(out, value);
    out << '\n';
  }
  out << "moleculeCount " << p.molecules.size() << '\n';
  for (const auto& m : p.molecules)
  {
    out << "molecule " << m.definitionName << ' ' << m.charge << ' ' << m.status << ' '
        << m.productGroup << ' ' << m.electronOccupancy.size();
    for (int n : m.electronOccupancy) out << ' ' << n;
    for (double x : {m.positionNm[0], m.positionNm[1], m.positionNm[2], m.direction[0], m.direction[1],
                     m.direction[2], m.globalTimeNs, m.localTimeNs, m.properTimeNs, m.decayTimeNs,
                     m.kineticEnergyEv})
    {
      out << ' ';
      PutReal(out, x);
    }
    out << '\n';
  }
  out << "end\n";
  if (!out) Fail("output stream failed");
}

PrecursorPacket Read(std::istream& in, const std::string& expectedTag, const PacketLimits& limits)
{
  PrecursorPacket p;
  const std::string magic = NextToken(in, "magic");
  Require(magic == kMagic, "wrong magic '" + magic + "'");
  const long long version = GetInteger(in, "version");
  Require(version == kVersion, "unsupported version " + std::to_string(version));

  p.compatibilityTag = GetFieldToken(in, "compatibilityTag");
  Require(p.compatibilityTag == expectedTag, "incompatible packet: tag '" + p.compatibilityTag +
                                                 "' but this reader expects '" + expectedTag + "'");
  p.primaryParticle = GetFieldToken(in, "primaryParticle");
  p.primaryEnergyEv = GetField(in, "primaryEnergy_eV");
  p.energyDepositEv = GetField(in, "energyDeposit_eV");
  p.escapedEnergyEv = GetField(in, "escapedEnergy_eV");
  p.captureHalfSideNm = GetField(in, "captureHalfSide_nm");
  p.captureTimeNs = GetField(in, "captureTime_ns");

  ExpectKeyword(in, "moleculeCount");
  const std::size_t count = GetCount(in, "moleculeCount", limits.maxMolecules);
  p.molecules.reserve(std::min<std::size_t>(count, std::size_t{1} << 16));
  for (std::size_t i = 0; i < count; ++i)
  {
    PacketMolecule m;
    ExpectKeyword(in, "molecule");
    m.definitionName = NextToken(in, "molecule name");
    m.charge = static_cast<int>(GetInteger(in, "charge"));
    m.status = static_cast<int>(GetInteger(in, "status"));
    m.productGroup = static_cast<int>(GetCount(in, "productGroup", limits.maxMolecules));
    const std::size_t levels = GetCount(in, "occupancy level count", limits.maxOccupancyLevels);
    for (std::size_t k = 0; k < levels; ++k)
      m.electronOccupancy.push_back(static_cast<int>(GetInteger(in, "occupancy")));
    for (double& x : m.positionNm) x = GetReal(in, "position");
    for (double& x : m.direction) x = GetReal(in, "direction");
    m.globalTimeNs = GetReal(in, "globalTime");
    m.localTimeNs = GetReal(in, "localTime");
    m.properTimeNs = GetReal(in, "properTime");
    m.decayTimeNs = GetReal(in, "decayTime");
    m.kineticEnergyEv = GetReal(in, "kineticEnergy");
    p.molecules.push_back(std::move(m));
  }
  ExpectKeyword(in, "end");
  std::string extra;
  Require(!(in >> extra), "trailing data after 'end'");
  Validate(p, limits);
  return p;
}

PrecursorPacket Translate(const PrecursorPacket& packet, const std::array<double, 3>& offsetNm)
{
  Require(AllFinite(offsetNm), "non-finite translation offset");
  PrecursorPacket moved = packet;
  for (auto& m : moved.molecules)
    for (int axis = 0; axis < 3; ++axis) m.positionNm[axis] += offsetNm[axis];
  return moved;
}

}  // namespace ptirt
