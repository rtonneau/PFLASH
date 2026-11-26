#include "PhaseSpaceFormat.hh"

#include <bit>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace phsp
{

namespace
{

// Little-endian field encoders and decoders, byte by byte, so the result does
// not depend on the host byte order or struct layout.
void PutU32(std::uint8_t* out, std::uint32_t value)
{
  for (int i = 0; i < 4; ++i)
    out[i] = static_cast<std::uint8_t>(value >> (8 * i));
}

void PutU64(std::uint8_t* out, std::uint64_t value)
{
  for (int i = 0; i < 8; ++i)
    out[i] = static_cast<std::uint8_t>(value >> (8 * i));
}

void PutI32(std::uint8_t* out, std::int32_t value) { PutU32(out, static_cast<std::uint32_t>(value)); }

void PutF64(std::uint8_t* out, double value) { PutU64(out, std::bit_cast<std::uint64_t>(value)); }

std::uint32_t GetU32(const std::uint8_t* in)
{
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(in[i]) << (8 * i);
  return value;
}

std::uint64_t GetU64(const std::uint8_t* in)
{
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i)
    value |= static_cast<std::uint64_t>(in[i]) << (8 * i);
  return value;
}

std::int32_t GetI32(const std::uint8_t* in) { return static_cast<std::int32_t>(GetU32(in)); }

double GetF64(const std::uint8_t* in) { return std::bit_cast<double>(GetU64(in)); }

constexpr std::size_t kChecksumOffset = 96;
constexpr std::size_t kReservedOffset = 104;

std::uint64_t HeaderChecksum(const HeaderBytes& bytes)
{
  Fnv1a64 hash;
  hash.Update(bytes.data(), kChecksumOffset);
  return hash.Value();
}

void RequireFinite(double value, const char* field)
{
  if (!std::isfinite(value))
    throw std::runtime_error(std::string("phase-space ") + field + " is not finite");
}

}  // namespace

HeaderBytes EncodeHeader(const FileHeader& header)
{
  HeaderBytes bytes{};
  for (std::size_t i = 0; i < kMagic.size(); ++i)
    bytes[i] = static_cast<std::uint8_t>(kMagic[i]);
  PutU32(&bytes[8], header.formatVersion);
  PutU32(&bytes[12], static_cast<std::uint32_t>(kHeaderSize));
  PutU32(&bytes[16], static_cast<std::uint32_t>(kHistoryEntrySize));
  PutU32(&bytes[20], static_cast<std::uint32_t>(kRecordSize));
  PutU64(&bytes[24], header.fileId);
  PutU64(&bytes[32], header.contentDigest);
  PutU64(&bytes[40], header.stage1Seed);
  PutU64(&bytes[48], header.primariesFired);
  PutU64(&bytes[56], header.historiesWritten);
  PutU64(&bytes[64], header.recordsWritten);
  PutF64(&bytes[72], header.latticePitchNm);
  PutF64(&bytes[80], header.boxSideNm);
  PutF64(&bytes[88], header.depthNm);
  PutU64(&bytes[kChecksumOffset], HeaderChecksum(bytes));
  return bytes;  // bytes 104..127 stay zero (reserved)
}

FileHeader DecodeHeader(const HeaderBytes& bytes)
{
  for (std::size_t i = 0; i < kMagic.size(); ++i)
    if (bytes[i] != static_cast<std::uint8_t>(kMagic[i]))
      throw std::runtime_error("not a phase-space file (wrong magic)");
  FileHeader header;
  header.formatVersion = GetU32(&bytes[8]);
  if (header.formatVersion != kFormatVersion)
    throw std::runtime_error("unsupported phase-space format version " + std::to_string(header.formatVersion) +
                             " (this reader supports " + std::to_string(kFormatVersion) + ")");
  const std::uint32_t headerSize = GetU32(&bytes[12]);
  const std::uint32_t entrySize = GetU32(&bytes[16]);
  const std::uint32_t recordSize = GetU32(&bytes[20]);
  if (headerSize != kHeaderSize || entrySize != kHistoryEntrySize || recordSize != kRecordSize)
    throw std::runtime_error("phase-space block sizes " + std::to_string(headerSize) + "/" +
                             std::to_string(entrySize) + "/" + std::to_string(recordSize) +
                             " (header/history/record) differ from format v1 (" + std::to_string(kHeaderSize) +
                             "/" + std::to_string(kHistoryEntrySize) + "/" + std::to_string(kRecordSize) + ")");
  if (GetU64(&bytes[kChecksumOffset]) != HeaderChecksum(bytes))
    throw std::runtime_error("phase-space header checksum mismatch (corrupted header)");
  for (std::size_t i = kReservedOffset; i < kHeaderSize; ++i)
    if (bytes[i] != 0)
      throw std::runtime_error("phase-space header reserved byte " + std::to_string(i) + " is not zero");
  header.fileId = GetU64(&bytes[24]);
  header.contentDigest = GetU64(&bytes[32]);
  header.stage1Seed = GetU64(&bytes[40]);
  header.primariesFired = GetU64(&bytes[48]);
  header.historiesWritten = GetU64(&bytes[56]);
  header.recordsWritten = GetU64(&bytes[64]);
  header.latticePitchNm = GetF64(&bytes[72]);
  header.boxSideNm = GetF64(&bytes[80]);
  header.depthNm = GetF64(&bytes[88]);
  return header;
}

HistoryEntryBytes EncodeHistoryEntry(const HistoryEntry& entry)
{
  HistoryEntryBytes bytes{};
  PutU64(&bytes[0], entry.historyIndex);
  PutU64(&bytes[8], entry.stage1EventId);
  PutU64(&bytes[16], entry.firstRecord);
  PutU64(&bytes[24], entry.recordCount);
  PutF64(&bytes[32], entry.inBoxDepositEv);
  PutI32(&bytes[40], entry.boxIx);
  PutI32(&bytes[44], entry.boxIy);
  return bytes;
}

HistoryEntry DecodeHistoryEntry(const HistoryEntryBytes& bytes)
{
  HistoryEntry entry;
  entry.historyIndex = GetU64(&bytes[0]);
  entry.stage1EventId = GetU64(&bytes[8]);
  entry.firstRecord = GetU64(&bytes[16]);
  entry.recordCount = GetU64(&bytes[24]);
  entry.inBoxDepositEv = GetF64(&bytes[32]);
  entry.boxIx = GetI32(&bytes[40]);
  entry.boxIy = GetI32(&bytes[44]);
  return entry;
}

RecordBytes EncodeRecord(const PhaseSpaceRecord& record)
{
  RecordBytes bytes{};
  PutI32(&bytes[0], record.pdg);
  PutI32(&bytes[4], record.trackId);
  PutF64(&bytes[8], record.kineticEnergyEv);
  for (std::size_t i = 0; i < 3; ++i)
    PutF64(&bytes[16 + 8 * i], record.positionNm[i]);
  for (std::size_t i = 0; i < 3; ++i)
    PutF64(&bytes[40 + 8 * i], record.direction[i]);
  PutF64(&bytes[64], record.timeNs);
  PutF64(&bytes[72], record.weight);
  return bytes;
}

PhaseSpaceRecord DecodeRecord(const RecordBytes& bytes)
{
  PhaseSpaceRecord record;
  record.pdg = GetI32(&bytes[0]);
  record.trackId = GetI32(&bytes[4]);
  record.kineticEnergyEv = GetF64(&bytes[8]);
  for (std::size_t i = 0; i < 3; ++i)
    record.positionNm[i] = GetF64(&bytes[16 + 8 * i]);
  for (std::size_t i = 0; i < 3; ++i)
    record.direction[i] = GetF64(&bytes[40 + 8 * i]);
  record.timeNs = GetF64(&bytes[64]);
  record.weight = GetF64(&bytes[72]);
  return record;
}

void ValidateHeader(const FileHeader& header)
{
  RequireFinite(header.latticePitchNm, "header latticePitchNm");
  RequireFinite(header.boxSideNm, "header boxSideNm");
  RequireFinite(header.depthNm, "header depthNm");
  if (header.boxSideNm <= 0)
    throw std::runtime_error("phase-space header boxSideNm must be > 0");
  if (header.latticePitchNm < 0 || (header.latticePitchNm > 0 && header.latticePitchNm < header.boxSideNm))
    throw std::runtime_error("phase-space header latticePitchNm must be 0 (single box) or >= boxSideNm "
                             "(non-overlapping boxes)");
  // One history per (primary, box): only a single box bounds the histories by the primaries.
  if (header.latticePitchNm == 0 && header.historiesWritten > header.primariesFired)
    throw std::runtime_error("phase-space header has more histories written (" +
                             std::to_string(header.historiesWritten) + ") than primaries fired (" +
                             std::to_string(header.primariesFired) + ") with a single box (pitch 0)");
  if (header.recordsWritten < header.historiesWritten)
    throw std::runtime_error("phase-space header has fewer records than histories (every history has >= 1 record)");
  if (header.fileId != DeriveFileId(header.stage1Seed, header.contentDigest))
    throw std::runtime_error("phase-space header fileId " + FormatId(header.fileId) +
                             " does not match its stage-1 seed and content digest");
}

void ValidateRecord(const PhaseSpaceRecord& record)
{
  if (record.pdg == 0)
    throw std::runtime_error("phase-space record has PDG code 0");
  if (record.trackId < 1)
    throw std::runtime_error("phase-space record has track ID " + std::to_string(record.trackId) + " (must be >= 1)");
  RequireFinite(record.kineticEnergyEv, "record kineticEnergyEv");
  for (double value : record.positionNm)
    RequireFinite(value, "record position");
  for (double value : record.direction)
    RequireFinite(value, "record direction");
  RequireFinite(record.timeNs, "record timeNs");
  RequireFinite(record.weight, "record weight");
  if (record.kineticEnergyEv < 0)
    throw std::runtime_error("phase-space record has a negative kinetic energy");
  if (record.weight <= 0)
    throw std::runtime_error("phase-space record weight must be > 0");
  const auto& d = record.direction;
  const double norm = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (std::abs(norm - 1.0) > kDirectionNormTolerance)
    throw std::runtime_error("phase-space record direction is not a unit vector (|d| = " + std::to_string(norm) + ")");
}

void Fnv1a64::Update(const std::uint8_t* data, std::size_t size)
{
  for (std::size_t i = 0; i < size; ++i)
  {
    fState ^= data[i];
    fState *= kPrime;
  }
}

std::uint64_t SplitMix64(std::uint64_t x)
{
  std::uint64_t z = x + 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

std::uint64_t DeriveFileId(std::uint64_t stage1Seed, std::uint64_t contentDigest)
{
  return SplitMix64(stage1Seed ^ SplitMix64(contentDigest));
}

std::string FormatId(std::uint64_t id)
{
  char buffer[19];
  std::snprintf(buffer, sizeof(buffer), "0x%016llx", static_cast<unsigned long long>(id));
  return buffer;
}

}  // namespace phsp
