// HistorySeed: the per-history seed of the stage-2 replay (ADR 0015,
// Geant4-free).
//
// Stage 2 reseeds the engine at each event from (stage-2 base seed, file ID,
// history index), so a Phase-space history gets the same physical stage in any
// run, range split or thread count, and under any chemistry variant.
//
// The mix is fixed (changing it changes every stage-2 result) and uses only
// unsigned 64-bit arithmetic, so it is the same on every platform:
//
//   HistorySeed(b, f, i) = S(S(S(b) ^ f) ^ i),   S = SplitMix64 (PhaseSpaceFormat.hh)
//
// HistoryEngineSeeds turns it into what G4Random::setTheSeeds(seeds) takes:
//   seeds[0] = bits 32..62 of the seed, seeds[1] = bits 0..30, each a positive
//   31-bit value (0 is replaced by 1, as 0 terminates a seed list), seeds[2] = 0.
// MixMaxRng::setSeeds (the Geant4 default engine) and RanecuEngine::setSeeds
// read the first two values. MixMaxRng keeps the pointer it was given (for
// getTheSeeds), so keep the array alive while the engine may report it, e.g.
// as a member of the primary generator.
#pragma once

#include <array>
#include <cstdint>

namespace phsp
{

std::uint64_t HistorySeed(std::uint64_t baseSeed, std::uint64_t fileId, std::uint64_t historyIndex);

std::array<long, 3> HistoryEngineSeeds(std::uint64_t baseSeed, std::uint64_t fileId, std::uint64_t historyIndex);

}  // namespace phsp
