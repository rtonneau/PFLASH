// Byte layout, encode/decode round trips and validation of format v1.
#include "PhaseSpaceFormat.hh"

#include <cmath>
#include <cstring>
#include <limits>

#include "TestCheck.hh"

using namespace phsp;

namespace
{

std::uint64_t Fnv(const char* text)
{
  Fnv1a64 hash;
  hash.Update(reinterpret_cast<const std::uint8_t*>(text), std::strlen(text));
  return hash.Value();
}

FileHeader MakeHeader()
{
  FileHeader h;
  h.contentDigest = 0x0123456789abcdefULL;
  h.stage1Seed = 1;
  h.primariesFired = 1000;
  h.historiesWritten = 7;
  h.recordsWritten = 31;
  h.latticePitchNm = 100000.0;
  h.boxSideNm = 4000.0;
  h.depthNm = 1.0e6;
  h.fileId = DeriveFileId(h.stage1Seed, h.contentDigest);
  return h;
}

PhaseSpaceRecord MakeRecord()
{
  PhaseSpaceRecord r;
  r.pdg = 11;
  r.trackId = 42;
  r.kineticEnergyEv = 1.0 / 3.0 * 1e5;
  r.positionNm = {-2000.0, 1234.5678901234567, 0.1};
  r.direction = {0.6, 0.0, 0.8};
  r.timeNs = 3.3356409519815204e-3;
  r.weight = 1.0;
  return r;
}

void TestHashes()
{
  // Published FNV-1a 64 and splitmix64 reference values.
  CHECK(Fnv("") == 0xcbf29ce484222325ULL);
  CHECK(Fnv("a") == 0xaf63dc4c8601ec8cULL);
  CHECK(Fnv("foobar") == 0x85944171f73967e8ULL);
  CHECK(SplitMix64(0) == 0xe220a8397b1dcdafULL);
  CHECK(SplitMix64(1) == 0x910a2dec89025cc1ULL);
  // Pinned (independent Python implementation).
  CHECK(DeriveFileId(1, 0x0123456789abcdefULL) == 0xd414adf34261bd2aULL);
  // Distinct seeds, same content: distinct IDs.
  CHECK(DeriveFileId(1, 99) != DeriveFileId(2, 99));
  CHECK(FormatId(0xd414adf34261bd2aULL) == "0xd414adf34261bd2a");
  CHECK(FormatId(1) == "0x0000000000000001");
}

void TestHeaderLayout()
{
  const FileHeader h = MakeHeader();
  const HeaderBytes bytes = EncodeHeader(h);
  CHECK(std::memcmp(bytes.data(), "PFPHSP\r\n", 8) == 0);
  CHECK(bytes[8] == 1 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0);  // version, little-endian
  CHECK(bytes[12] == 128 && bytes[16] == 48 && bytes[20] == 80);              // block sizes
  CHECK(bytes[24] == 0x2a && bytes[31] == 0xd4);                              // fileId LSB first
  CHECK(bytes[48] == 0xe8 && bytes[49] == 0x03);                              // primariesFired = 1000
  for (std::size_t i = 104; i < kHeaderSize; ++i)
    CHECK(bytes[i] == 0);
  CHECK(DecodeHeader(bytes) == h);
  ValidateHeader(h);
}

void TestHeaderErrors()
{
  const HeaderBytes good = EncodeHeader(MakeHeader());
  HeaderBytes bytes = good;
  bytes[0] = 'X';
  CHECK_THROWS(DecodeHeader(bytes), "wrong magic");
  bytes = good;
  bytes[8] = 2;
  CHECK_THROWS(DecodeHeader(bytes), "unsupported phase-space format version 2");
  bytes = good;
  bytes[20] = 81;
  CHECK_THROWS(DecodeHeader(bytes), "block sizes");
  bytes = good;
  bytes[80] ^= 0x01;  // box side changed: only the checksum can tell
  CHECK_THROWS(DecodeHeader(bytes), "header checksum mismatch");
  bytes = good;
  bytes[120] = 1;
  CHECK_THROWS(DecodeHeader(bytes), "reserved byte 120");

  FileHeader h = MakeHeader();
  h.latticePitchNm = 3999.0;
  CHECK_THROWS(ValidateHeader(h), "latticePitchNm");
  h = MakeHeader();
  h.latticePitchNm = 0;  // single box
  ValidateHeader(h);
  h.boxSideNm = 0;
  CHECK_THROWS(ValidateHeader(h), "boxSideNm must be > 0");
  h = MakeHeader();
  h.depthNm = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS(ValidateHeader(h), "depthNm is not finite");
  h = MakeHeader();
  h.historiesWritten = 1001;
  h.recordsWritten = 2000;
  ValidateHeader(h);  // a lattice: one primary may give a history in each of several boxes
  h.latticePitchNm = 0;
  CHECK_THROWS(ValidateHeader(h), "more histories written");
  h = MakeHeader();
  h.recordsWritten = 6;
  CHECK_THROWS(ValidateHeader(h), "fewer records than histories");
  h = MakeHeader();
  h.stage1Seed = 2;
  CHECK_THROWS(ValidateHeader(h), "does not match");
}

void TestEntryAndRecord()
{
  const HistoryEntry e{3, 17, 12, 5, 1234.25, -2, 7};
  const HistoryEntryBytes eb = EncodeHistoryEntry(e);
  CHECK(eb[0] == 3 && eb[8] == 17 && eb[16] == 12 && eb[24] == 5);
  CHECK(eb[40] == 0xfe && eb[43] == 0xff && eb[44] == 7 && eb[47] == 0);  // box (-2, 7), i32 little-endian
  CHECK(DecodeHistoryEntry(eb) == e);
  // Order: stage-1 event ID, then box iy, then box ix.
  CHECK(HistoryOrderKey(3, 5, 0) < HistoryOrderKey(3, -5, 1));
  CHECK(HistoryOrderKey(3, -1, 1) < HistoryOrderKey(3, 0, 1));
  CHECK(HistoryOrderKey(3, 9, 9) < HistoryOrderKey(4, -9, -9));

  PhaseSpaceRecord r = MakeRecord();
  RecordBytes rb = EncodeRecord(r);
  CHECK(rb[0] == 11 && rb[1] == 0 && rb[4] == 42);
  CHECK(DecodeRecord(rb) == r);  // bit-exact doubles
  r.pdg = -11;
  rb = EncodeRecord(r);
  CHECK(rb[0] == 0xf5 && rb[3] == 0xff);  // two's complement, little-endian
  CHECK(DecodeRecord(rb).pdg == -11);
  ValidateRecord(r);
}

void TestRecordErrors()
{
  PhaseSpaceRecord r = MakeRecord();
  r.pdg = 0;
  CHECK_THROWS(ValidateRecord(r), "PDG code 0");
  r = MakeRecord();
  r.trackId = 0;
  CHECK_THROWS(ValidateRecord(r), "track ID 0");
  r = MakeRecord();
  r.kineticEnergyEv = -1;
  CHECK_THROWS(ValidateRecord(r), "negative kinetic energy");
  r = MakeRecord();
  r.weight = 0;
  CHECK_THROWS(ValidateRecord(r), "weight must be > 0");
  r = MakeRecord();
  r.direction = {0, 0, 1.01};
  CHECK_THROWS(ValidateRecord(r), "not a unit vector");
  r = MakeRecord();
  r.positionNm[1] = std::numeric_limits<double>::infinity();
  CHECK_THROWS(ValidateRecord(r), "position is not finite");
}

}  // namespace

int main()
{
  TestHashes();
  TestHeaderLayout();
  TestHeaderErrors();
  TestEntryAndRecord();
  TestRecordErrors();
  return 0;
}
