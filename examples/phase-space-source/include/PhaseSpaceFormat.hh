// PhaseSpaceFormat: the binary phase-space file, format v1 (Geant4-free).
//
// A Phase-space file is `<name>.phsp` plus a `<name>.json` sidecar (written by
// PhaseSpaceWriter, informational: the reader uses the binary file only). The
// binary file is three blocks, every field little-endian with a fixed width,
// encoded field by field (never a memcpy of a struct):
//
//   file header    kHeaderSize bytes (offsets below)
//   history table  historiesWritten x kHistoryEntrySize bytes
//   records        recordsWritten    x kRecordSize bytes, grouped by history
//
// File header (128 bytes):
//    0  char[8] magic "PFPHSP\r\n" (the CR LF catches a text-mode copy)
//    8  u32 formatVersion (kFormatVersion)
//   12  u32 headerSize (kHeaderSize)
//   16  u32 historyEntrySize (kHistoryEntrySize)
//   20  u32 recordSize (kRecordSize)
//   24  u64 fileId          DeriveFileId(stage1Seed, contentDigest)
//   32  u64 contentDigest   FNV-1a 64 over the history-table and record bytes, in file order
//   40  u64 stage1Seed
//   48  u64 primariesFired  every stage-1 primary, including those with no record
//   56  u64 historiesWritten
//   64  u64 recordsWritten
//   72  f64 latticePitchNm  0 = single box; else >= boxSideNm
//   80  f64 boxSideNm       full side of the cubic scoring box, > 0
//   88  f64 depthNm         depth of the box centre in the phantom
//   96  u64 headerChecksum  FNV-1a 64 over header bytes 0..95 (the digest does not cover the header)
//  104  24 reserved bytes, zero
//
// History table entry (40 bytes), one per written history, in stage-1 event ID
// order (strictly increasing); historyIndex counts 0..N-1:
//    0  u64 historyIndex
//    8  u64 stage1EventId
//   16  u64 firstRecord     index of its first record; histories are contiguous
//   24  u64 recordCount     >= 1 (a primary with no record is not written)
//   32  f64 inBoxDepositEv  stage-1 energy deposited in the boxes by this history
//
// Record (80 bytes), one Inward crossing, in the frame of the box it entered
// (origin at the box centre, axes of the phantom):
//    0  i32 pdg (!= 0)
//    4  i32 trackId (>= 1)
//    8  f64 kineticEnergyEv (>= 0)
//   16  f64 x, y, z (nm)
//   40  f64 directionX, Y, Z (unit vector)
//   64  f64 timeNs (stage-1 global time)
//   72  f64 weight (> 0)
//
// Units are fixed by the field names: eV, nm, ns.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace phsp
{

inline constexpr std::array<char, 8> kMagic = {'P', 'F', 'P', 'H', 'S', 'P', '\r', '\n'};
inline constexpr std::uint32_t kFormatVersion = 1;
inline constexpr std::size_t kHeaderSize = 128;
inline constexpr std::size_t kHistoryEntrySize = 40;
inline constexpr std::size_t kRecordSize = 80;

// Relative tolerance on |direction| - 1 accepted by ValidateRecord.
inline constexpr double kDirectionNormTolerance = 1e-6;

struct FileHeader
{
  std::uint32_t formatVersion = kFormatVersion;
  std::uint64_t fileId = 0;
  std::uint64_t contentDigest = 0;
  std::uint64_t stage1Seed = 0;
  std::uint64_t primariesFired = 0;
  std::uint64_t historiesWritten = 0;
  std::uint64_t recordsWritten = 0;
  double latticePitchNm = 0;
  double boxSideNm = 0;
  double depthNm = 0;
  bool operator==(const FileHeader&) const = default;
};

struct HistoryEntry
{
  std::uint64_t historyIndex = 0;
  std::uint64_t stage1EventId = 0;
  std::uint64_t firstRecord = 0;
  std::uint64_t recordCount = 0;
  double inBoxDepositEv = 0;
  bool operator==(const HistoryEntry&) const = default;
};

struct PhaseSpaceRecord
{
  std::int32_t pdg = 0;
  std::int32_t trackId = 0;
  double kineticEnergyEv = 0;
  std::array<double, 3> positionNm = {0, 0, 0};
  std::array<double, 3> direction = {0, 0, 1};
  double timeNs = 0;
  double weight = 1;
  bool operator==(const PhaseSpaceRecord&) const = default;
};

using HeaderBytes = std::array<std::uint8_t, kHeaderSize>;
using HistoryEntryBytes = std::array<std::uint8_t, kHistoryEntrySize>;
using RecordBytes = std::array<std::uint8_t, kRecordSize>;

// Encoding writes the fixed sizes of the current format version.
HeaderBytes EncodeHeader(const FileHeader& header);
HistoryEntryBytes EncodeHistoryEntry(const HistoryEntry& entry);
RecordBytes EncodeRecord(const PhaseSpaceRecord& record);

// EncodeHeader fills in the header checksum. DecodeHeader throws
// std::runtime_error on a wrong magic, an unsupported version, block sizes
// other than this version's, a header checksum mismatch, or non-zero
// reserved bytes. It does not check the header values (ValidateHeader).
FileHeader DecodeHeader(const HeaderBytes& bytes);
HistoryEntry DecodeHistoryEntry(const HistoryEntryBytes& bytes);
PhaseSpaceRecord DecodeRecord(const RecordBytes& bytes);

// Throw std::runtime_error with a message naming the offending field.
// ValidateHeader checks the values (finite geometry, box side > 0, pitch 0 or
// >= box side, histories <= primaries) and that fileId matches DeriveFileId.
void ValidateHeader(const FileHeader& header);
// pdg != 0, trackId >= 1, finite fields, energy >= 0, weight > 0, unit direction.
void ValidateRecord(const PhaseSpaceRecord& record);

// FNV-1a 64-bit, incremental: Fnv1a64 h; h.Update(p, n); ... h.Value().
class Fnv1a64
{
public:
  static constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;
  static constexpr std::uint64_t kPrime = 0x00000100000001b3ULL;
  void Update(const std::uint8_t* data, std::size_t size);
  std::uint64_t Value() const { return fState; }

private:
  std::uint64_t fState = kOffsetBasis;
};

// The splitmix64 output function of state `x` (Steele, Lea, Flood 2014): the
// state is advanced by the golden gamma 0x9E3779B97F4A7C15, then mixed. A
// bijection on 64-bit values; SplitMix64(0) = 0xE220A8397B1DCDAF.
std::uint64_t SplitMix64(std::uint64_t x);

// fileId = SplitMix64(stage1Seed ^ SplitMix64(contentDigest)). For a fixed
// content, distinct seeds always give distinct IDs (SplitMix64 is a
// bijection); for distinct contents a clash has probability ~2^-64. The same
// seed and the same content (the same file written twice) give the same ID.
std::uint64_t DeriveFileId(std::uint64_t stage1Seed, std::uint64_t contentDigest);

// "0x" + 16 lower-case hex digits.
std::string FormatId(std::uint64_t id);

}  // namespace phsp
