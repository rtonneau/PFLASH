// PacketDigest: Geant4-free helpers that make the output of `capture`
// comparable between runs and between builds.
//
// 1. DigestMolecules: a 64-bit fingerprint of a set of molecules, independent
//    of the order they are listed in. `capture` writes two of them per packet to
//    capture.json: the digest of the molecules waiting at chemistry start and
//    the digest of the molecules that were captured. They are equal when the
//    capture is taken at chemistry start (patched build) and differ when the
//    start-up contacts of the stock build have already merged or moved
//    molecules.
// 2. CanonicalLess: the total order on molecules that the digest sorts by, also
//    used by `capture` to write the stock packet in an order that does not
//    depend on Geant4 track IDs or memory addresses.
// 3. ChemistrySeedFor: the seed of the chemistry stage in the stock build.
//
// The digest is FNV-1a (64 bit, offset 0xcbf29ce484222325, prime 0x100000001b3)
// over, in this order: the molecule count as 8 bytes, then for every molecule in
// canonical order its definition name bytes, a 0 byte, and the raw IEEE-754 bits
// of x, y, z (nm, relative to the packet origin) and the global time (ns), each
// as 8 bytes, least significant byte first. Only these five values enter: two
// molecules that differ in a single bit of a position or in the name give
// different digests, charge, direction and the product group do not.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HistoryConfig.hh"  // kMaxHistorySeed
#include "PrecursorPacket.hh"

namespace ptirt
{

// Order-independent FNV-1a 64 digest of the molecules (see above).
std::uint64_t DigestMolecules(const std::vector<PacketMolecule>& molecules);

// 16 lowercase hexadecimal digits (a JSON number could not hold all 64 bits).
std::string DigestToHex(std::uint64_t digest);

// Strict total order: definition name, then x, y, z, global time, each compared
// as an IEEE-754 value with -0 before +0 and NaNs ordered by their bits.
bool CanonicalLess(const PacketMolecule& a, const PacketMolecule& b);

// The stock build cannot reset the chemistry stage's random sequence from the
// history, so `capture` reseeds G4Random when the chemistry stage starts, with a
// seed derived from the physics seed of the event:
//   ChemistrySeedFor(physicsSeed) = kMaxHistorySeed + 1 - physicsSeed
// Physics seeds up to kMaxStockPhysicsSeed keep the two seed ranges apart
// (physics 1..1073741823, chemistry 1073741825..2147483647), and both stay
// inside the 32-bit range G4Random::setTheSeed takes on Windows.
inline constexpr std::uint64_t kMaxStockPhysicsSeed = kMaxHistorySeed / 2;  // 1073741823

// Throws std::out_of_range for a seed outside 1..kMaxStockPhysicsSeed.
std::uint64_t ChemistrySeedFor(std::uint64_t physicsSeed);

}  // namespace ptirt
