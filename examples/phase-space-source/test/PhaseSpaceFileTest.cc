// Writer + reader: file round trip, sidecar, writer refusals, the reader's
// errors on truncated, corrupted and inconsistent files, and the stage-2
// start times (t = 0 shift).
//
//   PhaseSpaceFileTest <output dir>
// Leaves <output dir>/roundtrip.phsp (+ .json) for the phsp-dump tests and the
// multi-file replay test, and <output dir>/box-5um.phsp (a 5 um box side) for
// the box-side mismatch replay test.
#include "PhaseSpaceReader.hh"
#include "PhaseSpaceWriter.hh"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "TestCheck.hh"

using namespace phsp;
namespace fs = std::filesystem;

namespace
{

PhaseSpaceRecord MakeRecord(int pdg, int trackId, double energyEv, double x)
{
  PhaseSpaceRecord r;
  r.pdg = pdg;
  r.trackId = trackId;
  r.kineticEnergyEv = energyEv;
  r.positionNm = {x, -x / 3.0, -2000.0};
  r.direction = {0.0, 0.6, 0.8};
  r.timeNs = 0.001 * trackId + 1.0 / 7.0;
  r.weight = 1.0;
  return r;
}

// Event 3 enters two lattice boxes, (0, 0) then (-1, 1): two histories.
std::vector<RecordedHistory> MakeHistories()
{
  return {
      {3, 1500.5, {MakeRecord(11, 1, 4.9e5, 10.0), MakeRecord(11, 7, 1234.5, -1999.0)}, 0, 0},
      {3, 0.0, {MakeRecord(22, 3, 2.0e4, 0.1)}, -1, 1},
      {11, 98765.4321, {MakeRecord(11, 1, 4.8e5, 1.0 / 3.0), MakeRecord(-11, 9, 7.0, 2.0), MakeRecord(11, 12, 0.0, 5.0)}, 0, 0},
  };
}

FileDescription MakeDescription(std::uint64_t seed = 20261126)
{
  FileDescription d;
  d.stage1Seed = seed;
  d.primariesFired = 50;
  d.latticePitchNm = 100000.0;
  d.boxSideNm = 4000.0;
  d.depthNm = 1.0e6;
  return d;
}

std::string ReadText(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteText(const fs::path& path, const std::string& bytes)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  CHECK(out.good());
}

void TestRoundTrip(const fs::path& dir)
{
  const auto histories = MakeHistories();
  DataNode stage1 = DataNode::MakeObject();
  stage1.Add("particle", "e-");
  stage1.Add("energy_keV", 500.0);
  const fs::path path = dir / "roundtrip.phsp";
  const WrittenFile written = WritePhaseSpaceFile(path, MakeDescription(), histories, stage1);
  CHECK(written.header.historiesWritten == 3);
  CHECK(written.header.recordsWritten == 6);
  CHECK(written.fileSizeBytes == kHeaderSize + 3 * kHistoryEntrySize + 6 * kRecordSize);
  CHECK(fs::file_size(path) == written.fileSizeBytes);
  CHECK(written.sidecarPath == dir / "roundtrip.json");
  CHECK(!fs::exists(dir / "roundtrip.phsp.tmp") && !fs::exists(dir / "roundtrip.json.tmp"));

  const PhaseSpaceReader reader(path);
  CHECK(reader.Header() == written.header);
  CHECK(reader.Header().primariesFired == 50 && reader.Header().stage1Seed == 20261126);
  CHECK(reader.Header().latticePitchNm == 100000.0 && reader.Header().boxSideNm == 4000.0 &&
        reader.Header().depthNm == 1.0e6);
  CHECK(reader.HistoryCount() == 3);
  std::uint64_t first = 0;
  for (std::uint64_t i = 0; i < 3; ++i)
  {
    const PhaseSpaceHistory h = reader.ReadHistory(i);
    CHECK(h.fileId == written.header.fileId);
    CHECK(h.entry.historyIndex == i);
    CHECK(h.entry.stage1EventId == histories[i].stage1EventId);
    CHECK(h.entry.boxIx == histories[i].boxIx && h.entry.boxIy == histories[i].boxIy);
    CHECK(h.entry.inBoxDepositEv == histories[i].inBoxDepositEv);
    CHECK(h.entry.firstRecord == first);
    CHECK(h.records == histories[i].records);  // bit-exact
    first += h.entry.recordCount;
  }
  CHECK_THROWS(reader.ReadHistory(3), "history 3 does not exist");

  const std::string json = ReadText(written.sidecarPath);
  CHECK(json.find("\"fileId\": \"" + FormatId(written.header.fileId) + "\"") != std::string::npos);
  CHECK(json.find("\"contentDigest\": \"" + FormatId(written.header.contentDigest) + "\"") != std::string::npos);
  CHECK(json.find("\"primariesFired\": 50") != std::string::npos);
  CHECK(json.find("\"historiesWritten\": 3") != std::string::npos);
  CHECK(json.find("\"recordsWritten\": 6") != std::string::npos);
  CHECK(json.find("\"totalInBoxDeposit_eV\": 100265.9321") != std::string::npos);
  CHECK(json.find("\"phspFile\": \"roundtrip.phsp\"") != std::string::npos);
  CHECK(json.find("\"particle\": \"e-\"") != std::string::npos);

  // Same content, another seed: another file ID.
  const WrittenFile other = WritePhaseSpaceFile(dir / "other-seed.phsp", MakeDescription(7), histories);
  CHECK(other.header.contentDigest == written.header.contentDigest);
  CHECK(other.header.fileId != written.header.fileId);
  // Writing again replaces the file; the content is deterministic.
  const WrittenFile again = WritePhaseSpaceFile(dir / "other-seed.phsp", MakeDescription(7), histories);
  CHECK(again.header == other.header);
}

void TestEmptyFile(const fs::path& dir)
{
  // Primaries fired, none crossed the box: a valid file with no history.
  const fs::path path = dir / "empty.phsp";
  const WrittenFile written = WritePhaseSpaceFile(path, MakeDescription(), {});
  CHECK(written.header.historiesWritten == 0 && written.header.recordsWritten == 0);
  const PhaseSpaceReader reader(path);
  CHECK(reader.HistoryCount() == 0);
  CHECK(reader.Header().primariesFired == 50);
}

void TestWriterRefusals(const fs::path& dir)
{
  const auto good = MakeHistories();
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.dat", MakeDescription(), good), "must end in .phsp");
  auto histories = good;
  histories[2].stage1EventId = 3;  // (3, box 0, 0) after (3, box -1, 1)
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), histories), "strictly increasing");
  histories = good;
  histories[1].boxIx = 0;  // same (event, box) twice
  histories[1].boxIy = 0;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), histories), "strictly increasing");
  histories = good;
  histories[1].records.clear();
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), histories), "has no record");
  histories = good;
  histories[0].inBoxDepositEv = -1;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), histories), "inBoxDepositEv");
  histories = good;
  histories[2].records[1].trackId = 0;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), histories), "record 1: phase-space record has track ID 0");
  FileDescription d = MakeDescription();
  d.primariesFired = 2;  // 3 histories from 2 primaries: fine on a lattice
  CHECK(WritePhaseSpaceFile(dir / "lattice-two-primaries.phsp", d, good).header.historiesWritten == 3);
  d.latticePitchNm = 0;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", d, good), "must be (0, 0)");
  const std::vector<RecordedHistory> singleBox = {good[0], good[2]};
  d.primariesFired = 1;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", d, singleBox), "more histories written");
  d.primariesFired = 2;
  CHECK(WritePhaseSpaceFile(dir / "single-box.phsp", d, singleBox).header.historiesWritten == 2);
  d = MakeDescription();
  d.latticePitchNm = 1000.0;
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", d, good), "latticePitchNm");
  CHECK_THROWS(WritePhaseSpaceFile(dir / "bad.phsp", MakeDescription(), good, DataNode(3)), "must be an object");
  CHECK(!fs::exists(dir / "bad.phsp") && !fs::exists(dir / "bad.json") && !fs::exists(dir / "bad.dat"));
}

// Write `bytes` as <name>.phsp and expect the reader to refuse it with `fragment`.
void ExpectRefused(const fs::path& dir, const std::string& name, const std::string& bytes, const std::string& fragment)
{
  const fs::path path = dir / (name + ".phsp");
  WriteText(path, bytes);
  CHECK_THROWS(PhaseSpaceReader reader(path), fragment);
  CHECK_THROWS(PhaseSpaceReader reader(path), "phase-space file '" + path.string() + "'");
}

void TestReaderErrors(const fs::path& dir)
{
  const std::string good = ReadText(dir / "roundtrip.phsp");
  const std::size_t tableEnd = kHeaderSize + 3 * kHistoryEntrySize;

  CHECK_THROWS(PhaseSpaceReader reader(dir / "missing.phsp"), "cannot read the file size");
  ExpectRefused(dir, "zero-bytes", "", "shorter than the 128-byte header");
  ExpectRefused(dir, "cut-header", good.substr(0, 100), "shorter than the 128-byte header");
  ExpectRefused(dir, "cut-table", good.substr(0, kHeaderSize + 50), "truncated: the header announces 3 histories");
  ExpectRefused(dir, "cut-last-byte", good.substr(0, good.size() - 1), "truncated");
  ExpectRefused(dir, "trailing", good + "x", "1 unexpected trailing bytes");
  ExpectRefused(dir, "text", std::string(200, 'x'), "wrong magic");

  std::string bytes = good;
  bytes[tableEnd + 2 * kRecordSize + 20] ^= 0x10;  // one bit of a record position
  ExpectRefused(dir, "flipped-record", bytes, "content digest mismatch");
  bytes = good;
  bytes[kHeaderSize + kHistoryEntrySize + 32] ^= 0x01;  // a history deposit
  ExpectRefused(dir, "flipped-table", bytes, "content digest mismatch");
  bytes = good;
  bytes[60] ^= 0x01;  // historiesWritten
  ExpectRefused(dir, "flipped-header", bytes, "header checksum mismatch");
  bytes = good;
  bytes[8] = 9;
  ExpectRefused(dir, "future-version", bytes, "unsupported phase-space format version 9");
}

// Re-seals a modified file image: recomputes the content digest over the
// table and records and the file ID, and re-encodes the header with its
// checksum, so the reader gets past the digest and reports the table problem.
std::string Reseal(std::string bytes, const FileHeader& header)
{
  Fnv1a64 hash;
  hash.Update(reinterpret_cast<const std::uint8_t*>(bytes.data()) + kHeaderSize, bytes.size() - kHeaderSize);
  FileHeader sealed = header;
  sealed.contentDigest = hash.Value();
  sealed.fileId = DeriveFileId(sealed.stage1Seed, sealed.contentDigest);
  const HeaderBytes headerBytes = EncodeHeader(sealed);
  bytes.replace(0, kHeaderSize, reinterpret_cast<const char*>(headerBytes.data()), kHeaderSize);
  return bytes;
}

// Replaces history-table entry `i` of a file image.
void PutEntry(std::string& bytes, std::size_t i, const HistoryEntry& entry)
{
  const HistoryEntryBytes entryBytes = EncodeHistoryEntry(entry);
  bytes.replace(kHeaderSize + i * kHistoryEntrySize, kHistoryEntrySize,
                reinterpret_cast<const char*>(entryBytes.data()), kHistoryEntrySize);
}

// The reader's own table checks, which the writer never lets through: entries
// out of (event ID, box iy, box ix) order, a duplicate key, and a box other
// than (0, 0) with pitch 0. Each file is re-sealed (digest, file ID, header
// checksum), so the refusal comes from the table check, not the digest.
void TestReaderTableRefusals(const fs::path& dir)
{
  const std::string good = ReadText(dir / "roundtrip.phsp");
  const PhaseSpaceReader reader(dir / "roundtrip.phsp");
  const FileHeader header = reader.Header();
  const std::vector<HistoryEntry> entries = reader.Histories();
  CHECK(entries.size() == 3);

  // The re-seal alone gives back the same, valid file.
  CHECK(Reseal(good, header) == good);

  // Entry 2 (event 11) moved to event 3, box (0, 0): (3, 0, 0) after (3, iy 1, ix -1).
  std::string bytes = good;
  HistoryEntry entry = entries[2];
  entry.stage1EventId = 3;
  PutEntry(bytes, 2, entry);
  ExpectRefused(dir, "table-out-of-order", Reseal(bytes, header),
                "history-table entry 2: (stage-1 event ID, box iy, box ix) is not strictly increasing");

  // Entry 1 given the key of entry 0: (3, 0, 0) twice.
  bytes = good;
  entry = entries[1];
  entry.boxIx = 0;
  entry.boxIy = 0;
  PutEntry(bytes, 1, entry);
  ExpectRefused(dir, "table-duplicate-key", Reseal(bytes, header),
                "history-table entry 1: (stage-1 event ID, box iy, box ix) is not strictly increasing");

  // Pitch 0 (single box) with entry 1 in box (-1, 1).
  FileHeader singleBox = header;
  singleBox.latticePitchNm = 0;
  ExpectRefused(dir, "table-pitch0-box", Reseal(good, singleBox),
                "history-table entry 1: box (-1, 1) with a single box (pitch 0); expected (0, 0)");
  // The same with every box at (0, 0) and distinct events is accepted.
  bytes = good;
  entry = entries[1];
  entry.stage1EventId = 5;
  entry.boxIx = 0;
  entry.boxIy = 0;
  PutEntry(bytes, 1, entry);
  WriteText(dir / "table-pitch0-ok.phsp", Reseal(bytes, singleBox));
  CHECK(PhaseSpaceReader(dir / "table-pitch0-ok.phsp").Header().latticePitchNm == 0);
}

// The stage-2 start times: record time minus the earliest time of the history.
void TestReplayTimes()
{
  CHECK(ReplayTimesNs({}).empty());
  std::vector<PhaseSpaceRecord> records = {MakeRecord(11, 1, 1.0, 0.0), MakeRecord(11, 2, 1.0, 0.0),
                                           MakeRecord(22, 3, 1.0, 0.0)};
  records[0].timeNs = 3.5;
  records[1].timeNs = 1.25;  // the earliest need not be the first record
  records[2].timeNs = 2.0;
  const std::vector<double> times = ReplayTimesNs(records);
  CHECK(times.size() == 3);
  CHECK(times[0] == 2.25 && times[1] == 0.0 && times[2] == 0.75);
  records.resize(1);
  CHECK(ReplayTimesNs(records) == std::vector<double>{0.0});
}

// A valid file with a 5 um box side (the round-trip file has 4 um), for the
// replay test that refuses files with different box sides.
void WriteOtherBoxSide(const fs::path& dir)
{
  FileDescription d = MakeDescription(99);
  d.boxSideNm = 5000.0;
  const WrittenFile written = WritePhaseSpaceFile(dir / "box-5um.phsp", d, MakeHistories());
  CHECK(PhaseSpaceReader(written.phspPath).Header().boxSideNm == 5000.0);
}

}  // namespace

int main(int argc, char** argv)
{
  CHECK(argc == 2);
  const fs::path dir = argv[1];
  fs::remove_all(dir);
  fs::create_directories(dir);
  TestRoundTrip(dir);
  TestEmptyFile(dir);
  TestWriterRefusals(dir);
  TestReaderErrors(dir);
  TestReaderTableRefusals(dir);
  TestReplayTimes();
  WriteOtherBoxSide(dir);
  return 0;
}
