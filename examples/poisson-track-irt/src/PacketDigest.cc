#include "PacketDigest.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace ptirt
{
namespace
{

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void MixByte(std::uint64_t& hash, unsigned char byte)
{
  hash ^= byte;
  hash *= kFnvPrime;
}

void MixWord(std::uint64_t& hash, std::uint64_t word)
{
  for (int shift = 0; shift < 64; shift += 8) MixByte(hash, static_cast<unsigned char>(word >> shift));
}

std::uint64_t Bits(double value)
{
  return std::bit_cast<std::uint64_t>(value);
}

// Maps the IEEE-754 bits so that unsigned comparison orders the values:
// negative numbers (sign bit set) are flipped, the others get the sign bit.
std::uint64_t OrderedBits(double value)
{
  const std::uint64_t bits = Bits(value);
  return (bits >> 63) != 0 ? ~bits : (bits | (std::uint64_t{1} << 63));
}

std::array<std::uint64_t, 4> OrderedKey(const PacketMolecule& m)
{
  return {OrderedBits(m.positionNm[0]), OrderedBits(m.positionNm[1]), OrderedBits(m.positionNm[2]),
          OrderedBits(m.globalTimeNs)};
}

}  // namespace

bool CanonicalLess(const PacketMolecule& a, const PacketMolecule& b)
{
  if (a.definitionName != b.definitionName) return a.definitionName < b.definitionName;
  return OrderedKey(a) < OrderedKey(b);
}

std::uint64_t DigestMolecules(const std::vector<PacketMolecule>& molecules)
{
  std::vector<const PacketMolecule*> sorted;
  sorted.reserve(molecules.size());
  for (const auto& m : molecules) sorted.push_back(&m);
  std::sort(sorted.begin(), sorted.end(),
            [](const PacketMolecule* a, const PacketMolecule* b) { return CanonicalLess(*a, *b); });

  std::uint64_t hash = kFnvOffset;
  MixWord(hash, molecules.size());
  for (const auto* m : sorted)
  {
    for (unsigned char c : m->definitionName) MixByte(hash, c);
    MixByte(hash, 0);
    for (double x : m->positionNm) MixWord(hash, Bits(x));
    MixWord(hash, Bits(m->globalTimeNs));
  }
  return hash;
}

std::string DigestToHex(std::uint64_t digest)
{
  static const char kDigits[] = "0123456789abcdef";
  std::string text(16, '0');
  for (int i = 15; i >= 0; --i)
  {
    text[static_cast<std::size_t>(i)] = kDigits[digest & 0xf];
    digest >>= 4;
  }
  return text;
}

std::uint64_t ChemistrySeedFor(std::uint64_t physicsSeed)
{
  if (physicsSeed < 1 || physicsSeed > kMaxStockPhysicsSeed)
    throw std::out_of_range("physics seed " + std::to_string(physicsSeed) +
                            " is outside 1.." + std::to_string(kMaxStockPhysicsSeed) +
                            " (the stock build derives the chemistry seed from it)");
  return kMaxHistorySeed + 1 - physicsSeed;
}

}  // namespace ptirt
