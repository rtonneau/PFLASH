// PrecursorPacket: the radiolytic molecules of ONE primary track, captured at a
// small "precursor age" (e.g. 1 ps) so they can later be injected, rigidly
// translated, into a shared IRT_syn chemistry history.
//
// Geant4-free: molecule definitions are stored by name.
//
// Units used everywhere in this header:
//   energy   eV      length   nm      time   ns
//
// Text format (version 2), one logical record per line, whitespace separated.
// Every real number is written as a C99 hexadecimal float ("%a", e.g.
// 0x1.8p+3), so Write followed by Read reproduces the exact bits.
//
//   PTIRT-PRECURSOR-PACKET 2
//   compatibilityTag <token>
//   primaryParticle <token>
//   primaryEnergy_eV <real>
//   energyDeposit_eV <real>
//   escapedEnergy_eV <real>
//   captureHalfSide_nm <real>
//   captureTime_ns <real>
//   moleculeCount <N>
//   molecule <name> <charge> <status> <productGroup> <nOcc> <occ...> <x y z nm>
//            <dx dy dz> <globalTime localTime properTime decayTime ns> <kineticEnergy eV>
//   ... (N lines)
//   end
//
// Tokens (tag, particle, molecule name) contain no whitespace.
//
// Version 2 added <productGroup> (version 1 packets are refused, not converted).
// productGroup is 0 for a molecule that is a precursor of the primary track, and
// k >= 1 for a molecule that a start-up reaction of the stock-build capture made
// (see CaptureRecorder.hh): all products of one reaction share the same k. The
// ids are group labels, not Geant4 reaction numbers: the capture writes them
// dense (1..G, no gap) in an order that does not depend on Geant4 track IDs.
// The reader checks only 0 <= k <= molecule count; density is a writer
// property. Products of one reaction sit at one point.
#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace ptirt
{

struct PacketMolecule
{
  std::string definitionName;                // G4MoleculeDefinition name, e.g. "OH"
  int charge = 0;
  int status = 0;                            // 0 or 1 (electronic-state switch)
  int productGroup = 0;                      // 0 = precursor; k >= 1 = product of start-up reaction k; <= molecule count
  std::vector<int> electronOccupancy;        // per-level occupancy, each 0..maxOccupancyPerLevel
  std::array<double, 3> positionNm{};        // relative to the packet origin (primary entry point)
  std::array<double, 3> direction{0, 0, 1};  // unit vector
  double globalTimeNs = 0;                   // >= 0
  double localTimeNs = 0;                    // >= 0
  double properTimeNs = 0;                   // >= 0
  double decayTimeNs = 0;                    // finite; Geant4's negative "stable" sentinel is allowed
  double kineticEnergyEv = 0;                // >= 0
  bool operator==(const PacketMolecule&) const = default;
};

struct PrecursorPacket
{
  std::string compatibilityTag;  // must equal the reader's expected tag (physics/chemistry/version)
  std::string primaryParticle;   // e.g. "e-"
  double primaryEnergyEv = 0;    // > 0
  double energyDepositEv = 0;    // 0..primaryEnergyEv: energy deposited in the capture volume
  double escapedEnergyEv = 0;    // energy that left the capture volume
  double captureHalfSideNm = 0;  // > 0, half side of the capture box
  double captureTimeNs = 0;      // precursor age, > 0 (e.g. 0.001 for 1 ps)
  std::vector<PacketMolecule> molecules;
  bool operator==(const PrecursorPacket&) const = default;
};

// Explicit, documented bounds (all configurable).
struct PacketLimits
{
  std::size_t maxMolecules = 10'000'000;
  std::size_t maxOccupancyLevels = 64;
  int maxOccupancyPerLevel = 16;
  // Energy balance: |deposit + escaped - primary| <= energyAbsTolEv + energyRelTol * primary.
  // The tolerance absorbs floating-point summation error, not physics: a primary whose
  // energy is not fully accounted for (e.g. lost to a killed secondary) is rejected.
  double energyRelTol = 1e-6;
  double energyAbsTolEv = 1e-3;
  double directionNormTol = 1e-6;  // | |d| - 1 | <= this
};

// Throw std::runtime_error with a clear message if the packet is invalid.
void Validate(const PrecursorPacket& packet, const PacketLimits& limits = {});

// Validate, then write. Throws on an invalid packet or a failed stream.
void Write(std::ostream& out, const PrecursorPacket& packet, const PacketLimits& limits = {});

// Read, then Validate. Throws on malformed, truncated, trailing-garbage,
// wrong magic/version, or compatibilityTag != expectedTag.
PrecursorPacket Read(std::istream& in, const std::string& expectedTag,
                     const PacketLimits& limits = {});

// Rigid translation of every molecule by offsetNm (the injector's job in the
// full example; provided here so it is testable). Ages, directions and energies
// are unchanged. Throws on a non-finite offset.
PrecursorPacket Translate(const PrecursorPacket& packet, const std::array<double, 3>& offsetNm);

}  // namespace ptirt
