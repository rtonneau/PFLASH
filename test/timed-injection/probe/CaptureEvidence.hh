#pragma once
#include "TrackPacket.hh"
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace Calibration
{
enum class Split { Calibration, Holdout, Fresh };
enum class Status { Unexecuted, Attempted, Accepted, Rejected, Censored };
struct SeedRecord
{
  std::string streamID;
  std::uint64_t seed = 0;
};
struct CaptureRecord
{
  std::string condition, captureID;
  Split split = Split::Calibration;
  std::uint64_t realization = 0, arrival = 0;
  // Order: physical transport, source position, precursor chemistry.
  std::array<std::uint64_t, 3> seeds{};
  Status status = Status::Unexecuted;
  bool intended = true, attempted = false;
  bool processCompleted = false, validationMarker = false, failureMarker = false;
  bool packetComplete = false, energyLedgerComplete = false, provenanceValid = false;
  bool aborted = false, truncated = false;
  int exitCode = -1, completedEvents = 0;
  double energyMeV = 0, escapedMeV = 0, totalMeV = 0;
  double massKg = 0, halfSide = 0;
  std::string compatibility, diagnostic;
};
// IDs and all random streams are frozen before execution. Retained seeds include
// earlier studies and non-capture streams (arrival, history chemistry, resampling).
std::vector<SeedRecord> AllocateSeeds(const std::vector<std::string> &streamIDs,
                                    std::uint64_t firstSeed,
                                    const std::vector<SeedRecord> &retained = {});
void ValidateIdentities(const std::vector<CaptureRecord> &records,
                        const std::vector<CaptureRecord> &retained = {});
void ValidateCapture(const CaptureRecord &, const Packets::Packet &);
Packets::Packet ReadCapture(std::istream &, const CaptureRecord &);
// Versioned, lossless text schema. Every intended record is serialized, including
// failed/nonfinite diagnostic evidence; no filtering to the accepted subset.
// Header: PFLASH_CAPTURE_LEDGER 1 count. Rows in order: quoted condition,
// quoted captureID, split, realization, arrival, three seeds, status,
// ten 0/1 flags in CaptureRecord declaration order, exitCode, completedEvents,
// five IEEE-754 uint64 bit patterns (energy, escaped, total, mass, halfSide),
// quoted compatibility, quoted diagnostic. Quoted strings may contain newlines.
void WriteLedger(std::ostream &, const std::vector<CaptureRecord> &);
std::vector<CaptureRecord> ReadLedger(std::istream &);
// Supply the frozen protocol's intended count, never the accepted subset size.
// This is a ledger completeness gate; accepted rows still require ReadCapture.
bool CompleteSample(const std::vector<CaptureRecord> &, std::size_t intendedCount);
} // namespace Calibration
