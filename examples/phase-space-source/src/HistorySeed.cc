#include "HistorySeed.hh"

#include "PhaseSpaceFormat.hh"

namespace phsp
{

std::uint64_t HistorySeed(std::uint64_t baseSeed, std::uint64_t fileId, std::uint64_t historyIndex)
{
  return SplitMix64(SplitMix64(SplitMix64(baseSeed) ^ fileId) ^ historyIndex);
}

std::array<long, 3> HistoryEngineSeeds(std::uint64_t baseSeed, std::uint64_t fileId, std::uint64_t historyIndex)
{
  const std::uint64_t seed = HistorySeed(baseSeed, fileId, historyIndex);
  constexpr std::uint64_t kMask31 = 0x7FFFFFFFULL;
  const auto positive = [](std::uint64_t value) { return static_cast<long>(value == 0 ? 1 : value); };
  return {positive((seed >> 32) & kMask31), positive(seed & kMask31), 0L};
}

}  // namespace phsp
