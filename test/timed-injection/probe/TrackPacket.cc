#include "TrackPacket.hh"
#include <bit>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
namespace Packets
{
namespace
{
void Require(bool ok)
{
  if (!ok)
    throw std::runtime_error("invalid packet");
}
void Put(std::ostream &out, double x) { out << std::bit_cast<std::uint64_t>(x) << ' '; }
double Get(std::istream &in)
{
  std::uint64_t x;
  Require(bool(in >> x));
  return std::bit_cast<double>(x);
}
} // namespace
void Validate(const Packet &p)
{
  Require(!p.compatibility.empty() && p.compatibility.size() < 4096);
  Require(std::isfinite(p.massKg) && p.massKg > 0 && std::isfinite(p.halfSide) && p.halfSide > 0);
  Require(std::isfinite(p.energyDeposit) && p.energyDeposit >= 0 &&
          p.energyDeposit <= 0.500000001 && std::isfinite(p.secondaryDeposit) &&
          p.secondaryDeposit >= 0 && p.secondaryDeposit <= p.energyDeposit);
  for (double x : p.entry)
    Require(std::isfinite(x) && std::abs(x) <= p.halfSide);
  Require(p.direction == std::array<double, 3>{0, 0, 1});
  Require(p.molecules.size() <= 1000000);
  for (const auto &m : p.molecules)
  {
    Require(!m.definition.empty() && m.definition.size() < 128 && (m.status == 0 || m.status == 1));
    Require(m.occupancy.size() <= 32);
    for (int n : m.occupancy)
      Require(n >= 0 && n <= 16);
    for (double x : m.position)
      Require(std::isfinite(x)); // Solvation displacement may cross the boundary.
    double norm = 0;
    for (double x : m.direction)
    {
      Require(std::isfinite(x));
      norm += x * x;
    }
    Require(std::abs(norm - 1) < 1e-12);
    for (double x : {m.globalTime, m.localTime, m.properTime, m.kineticEnergy})
      Require(std::isfinite(x) && x >= 0);
    Require(m.globalTime == 0.001 && m.localTime == 0 && m.properTime == 0);
    Require(std::isfinite(m.decayTime)); // Geant4's negative stable-species sentinel is preserved.
  }
}
void Write(std::ostream &out, const Packet &p)
{
  Validate(p);
  out << "PFLASH_PRECURSOR 1 " << std::quoted(p.compatibility) << '\n';
  for (double x : {p.massKg, p.halfSide, p.energyDeposit, p.secondaryDeposit})
    Put(out, x);
  for (double x : p.entry)
    Put(out, x);
  for (double x : p.direction)
    Put(out, x);
  out << p.molecules.size() << '\n';
  for (const auto &m : p.molecules)
  {
    out << std::quoted(m.definition) << ' ' << m.charge << ' ' << m.status << ' '
        << m.occupancy.size() << ' ';
    for (int n : m.occupancy)
      out << n << ' ';
    for (double x : m.position)
      Put(out, x);
    for (double x : m.direction)
      Put(out, x);
    for (double x : {m.globalTime, m.localTime, m.properTime, m.decayTime, m.kineticEnergy})
      Put(out, x);
    out << '\n';
  }
  Require(bool(out));
}
Packet Read(std::istream &in, const std::string &compatibility)
{
  Packet p;
  std::string magic;
  int version = 0;
  Require(bool(in >> magic >> version >> std::quoted(p.compatibility)) &&
          magic == "PFLASH_PRECURSOR" && version == 1 && p.compatibility == compatibility);
  p.massKg = Get(in);
  p.halfSide = Get(in);
  p.energyDeposit = Get(in);
  p.secondaryDeposit = Get(in);
  for (double &x : p.entry)
    x = Get(in);
  for (double &x : p.direction)
    x = Get(in);
  std::size_t count = 0;
  Require(bool(in >> count) && count <= 1000000);
  p.molecules.resize(count);
  for (auto &m : p.molecules)
  {
    std::size_t n = 0;
    Require(bool(in >> std::quoted(m.definition) >> m.charge >> m.status >> n) && n <= 32);
    m.occupancy.resize(n);
    for (int &x : m.occupancy)
      Require(bool(in >> x));
    for (double &x : m.position)
      x = Get(in);
    for (double &x : m.direction)
      x = Get(in);
    m.globalTime = Get(in);
    m.localTime = Get(in);
    m.properTime = Get(in);
    m.decayTime = Get(in);
    m.kineticEnergy = Get(in);
  }
  in >> std::ws;
  Require(in.eof());
  Validate(p);
  return p;
}
} // namespace Packets
