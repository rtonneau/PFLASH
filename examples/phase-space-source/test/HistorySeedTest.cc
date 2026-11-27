// Per-history seed (ADR 0015): pinned values, so a change of the mix fails here
// (it would change every stage-2 result). The expected values come from an
// independent implementation (Python, unsigned 64-bit arithmetic).
#include "HistorySeed.hh"

#include <set>

#include "TestCheck.hh"

using namespace phsp;

int main()
{
  constexpr std::uint64_t kFile = 0xfedcba9876543210ULL;

  CHECK(HistorySeed(0, 0, 0) == 0x238275bc38fcbe91ULL);
  CHECK(HistorySeed(1, 0, 0) == 0xb18a02f46d8d86c3ULL);
  CHECK(HistorySeed(12345, kFile, 0) == 0x4175630fed1e97b3ULL);
  CHECK(HistorySeed(12345, kFile, 1) == 0x88ff37d3ae7996d6ULL);
  CHECK(HistorySeed(12345, kFile, 999999) == 0x87641ec7ac8825f8ULL);

  auto seeds = HistoryEngineSeeds(0, 0, 0);
  CHECK(seeds[0] == 595752380L && seeds[1] == 956087953L && seeds[2] == 0L);
  seeds = HistoryEngineSeeds(12345, kFile, 0);
  CHECK(seeds[0] == 1098212111L && seeds[1] == 1830721459L && seeds[2] == 0L);
  seeds = HistoryEngineSeeds(12345, kFile, 999999);
  CHECK(seeds[0] == 124001991L && seeds[1] == 747120120L && seeds[2] == 0L);

  // The arguments are not interchangeable.
  CHECK(HistorySeed(12345, 1, 2) != HistorySeed(12345, 2, 1));
  CHECK(HistorySeed(1, 12345, 2) != HistorySeed(12345, 1, 2));

  // Engine seeds: two positive 31-bit values and a terminator; distinct pairs
  // over a range of histories.
  std::set<std::pair<long, long>> pairs;
  for (std::uint64_t i = 0; i < 10000; ++i)
  {
    const auto s = HistoryEngineSeeds(7, kFile, i);
    CHECK(s[0] >= 1 && s[0] <= 0x7FFFFFFFL);
    CHECK(s[1] >= 1 && s[1] <= 0x7FFFFFFFL);
    CHECK(s[2] == 0);
    pairs.insert({s[0], s[1]});
  }
  CHECK(pairs.size() == 10000);
  return 0;
}
