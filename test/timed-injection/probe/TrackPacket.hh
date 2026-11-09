#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace Packets
{
struct Molecule
{
  std::string definition;
  int charge = 0, status = 0;
  std::vector<int> occupancy;
  std::array<double, 3> position{}, direction{};
  double globalTime = 0, localTime = 0, properTime = 0, decayTime = 0, kineticEnergy = 0;
  bool operator==(const Molecule &) const = default;
};
struct Packet
{
  std::string compatibility;
  double massKg = 0, halfSide = 0, energyDeposit = 0, secondaryDeposit = 0;
  std::array<double, 3> entry{}, direction{0, 0, 1};
  std::vector<Molecule> molecules;
  bool operator==(const Packet &) const = default;
};
void Validate(const Packet &);
void Write(std::ostream &, const Packet &);
Packet Read(std::istream &, const std::string &compatibility);
} // namespace Packets
