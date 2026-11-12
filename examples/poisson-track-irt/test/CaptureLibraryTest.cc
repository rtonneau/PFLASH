#include "CaptureLibrary.hh"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ExamplePhysicsTag.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

PrecursorPacket MakePacket(double deposit, int molecules)
{
  PrecursorPacket p;
  p.compatibilityTag = kPacketCompatibilityTag;
  p.primaryParticle = "e-";
  p.primaryEnergyEv = 10000.0;
  p.energyDepositEv = deposit;
  p.escapedEnergyEv = 10000.0 - deposit;
  p.captureHalfSideNm = 10000.0;
  p.captureTimeNs = kPrecursorAgeNs;
  for (int i = 0; i < molecules; ++i)
  {
    PacketMolecule m;
    m.definitionName = i % 2 ? "H2O" : "e_aq";
    m.charge = i % 2 ? 1 : -1;
    m.status = i % 2;
    m.electronOccupancy = {2, 2, 2, 2, 1};
    m.positionNm = {1.0 / 3.0 + i, -2.0 * i, 0.5};
    m.direction = {0.0, 0.6, 0.8};
    m.globalTimeNs = kPrecursorAgeNs;
    m.decayTimeNs = -1.0;
    p.molecules.push_back(m);
  }
  return p;
}

std::string IndexText(const std::vector<LibraryEntry>& entries)
{
  std::ostringstream out;
  WriteLibraryIndex(out, entries);
  return out.str();
}

std::vector<LibraryEntry> ParseIndex(const std::string& text)
{
  std::istringstream in(text);
  return ReadLibraryIndex(in);
}

void ReplaceFirst(std::string& text, const std::string& from, const std::string& to)
{
  const auto pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), to);
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
  std::ofstream out(path, std::ios::binary);
  out << text;
  CHECK(static_cast<bool>(out));
}

void TestFileNamesAndEntry()
{
  CHECK(PacketFileName(0) == "packet-00000.txt");
  CHECK(PacketFileName(7) == "packet-00007.txt");
  CHECK(PacketFileName(123456) == "packet-123456.txt");

  const auto packet = MakePacket(9990.123456789, 3);
  const auto e = MakeLibraryEntry(4, 42, packet);
  CHECK(e.packetIndex == 4);
  CHECK(e.file == "packet-00004.txt");
  CHECK(e.physicsSeed == 42);
  CHECK(e.primaryParticle == "e-");
  CHECK(e.moleculeCount == 3);
  CHECK(e.energyResidualEv == e.energyDepositEv + e.escapedEnergyEv - e.primaryEnergyEv);
}

void TestIndexRoundTrip()
{
  std::vector<LibraryEntry> entries;
  entries.push_back(MakeLibraryEntry(0, 1, MakePacket(9990.123456789, 3)));
  entries.push_back(MakeLibraryEntry(1, 2, MakePacket(1.0 / 3.0, 5)));
  entries[1].secondTrial = SecondTrial{3, 1.0 / 7.0};  // one stock-style entry, one without
  const auto text = IndexText(entries);
  CHECK(text.rfind("packetIndex,file,physicsSeed,primaryParticle,primaryEnergyEv,energyDepositEv,"
                   "escapedEnergyEv,energyResidualEv,moleculeCount,secondTrialPairs,"
                   "secondTrialExpectedReactions\n",
                   0) == 0);
  CHECK(ParseIndex(text) == entries);

  // CRLF line endings are accepted.
  std::string crlf;
  for (char c : text)
  {
    if (c == '\n') crlf += '\r';
    crlf += c;
  }
  CHECK(ParseIndex(crlf) == entries);

  const auto totals = SumLibrary(entries);
  CHECK(totals.packetCount == 2);
  CHECK(totals.moleculeCount == 8);
  CHECK(totals.primaryEnergyEv == 20000.0);
}

void TestIndexWriteRejects()
{
  const auto good = MakeLibraryEntry(0, 1, MakePacket(9000.0, 1));
  auto e = good;
  e.packetIndex = 1;
  CHECK_THROWS(IndexText({e}), "row number");
  e = good;
  e.file = "../packet.txt";
  CHECK_THROWS(IndexText({e}), "unsafe packet file name");
  e = good;
  e.physicsSeed = 0;
  CHECK_THROWS(IndexText({e}), "seed");
  e = good;
  e.primaryParticle = "e,-";
  CHECK_THROWS(IndexText({e}), "plain token");
  e = good;
  e.energyResidualEv += 1.0;
  CHECK_THROWS(IndexText({e}), "residual");
}

void TestIndexReadRejects()
{
  std::vector<LibraryEntry> entries{MakeLibraryEntry(0, 1, MakePacket(9000.0, 1)),
                                    MakeLibraryEntry(1, 2, MakePacket(8000.0, 2))};
  const auto text = IndexText(entries);

  CHECK_THROWS(ParseIndex(""), "no header");
  CHECK_THROWS(ParseIndex(text.substr(0, text.find('\n') + 1)), "lists no packet");
  auto t = text;
  ReplaceFirst(t, "packetIndex", "index");
  CHECK_THROWS(ParseIndex(t), "unexpected header");
  t = text;
  ReplaceFirst(t, "\n1,", "\n2,");
  CHECK_THROWS(ParseIndex(t), "row number");
  t = text;
  ReplaceFirst(t, "packet-00001.txt", "packet-00000.txt");
  CHECK_THROWS(ParseIndex(t), "duplicate");
  t = text;
  ReplaceFirst(t, "packet-00001.txt", "sub/packet.txt");
  CHECK_THROWS(ParseIndex(t), "unsafe");
  t = text;
  ReplaceFirst(t, ",packet-00001.txt,2,", ",packet-00001.txt,0,");
  CHECK_THROWS(ParseIndex(t), "seed");
  t = text;
  ReplaceFirst(t, ",8000,", ",8000.5,");
  CHECK_THROWS(ParseIndex(t), "energyResidualEv");
  t = text;
  ReplaceFirst(t, ",8000,", ",abc,");
  CHECK_THROWS(ParseIndex(t), "malformed");
  t = text + "2,packet-00002.txt,3,e-,1\n";
  CHECK_THROWS(ParseIndex(t), "fields");
  t = text + "\n";
  CHECK_THROWS(ParseIndex(t), "blank line");
  // The two second-trial fields are both set or both empty.
  t = text;
  ReplaceFirst(t, ",2,,\n", ",2,3,\n");
  CHECK_THROWS(ParseIndex(t), "both be set or both empty");
  t = text;
  ReplaceFirst(t, ",2,,\n", ",2,3,-1\n");
  CHECK_THROWS(ParseIndex(t), "negative");
}

void TestLoadLibrary(const std::filesystem::path& scratch)
{
  const auto dir = scratch / "library";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  std::vector<PrecursorPacket> packets{MakePacket(9990.5, 3), MakePacket(10000.0, 4)};
  std::vector<LibraryEntry> entries;
  for (std::size_t i = 0; i < packets.size(); ++i)
  {
    std::ofstream out(dir / PacketFileName(i), std::ios::binary);
    Write(out, packets[i]);
    entries.push_back(MakeLibraryEntry(i, 100 + static_cast<long long>(i), packets[i]));
  }
  WriteFile(dir / kLibraryIndexName, IndexText(entries));

  const auto library = LoadPacketLibrary(dir, kPacketCompatibilityTag);
  CHECK(library.entries == entries);
  CHECK(library.packets == packets);

  // Another physics tag is rejected.
  CHECK_THROWS(LoadPacketLibrary(dir, "other-tag"), "packet-00000.txt");

  // A row that disagrees with its packet is rejected.
  auto wrong = entries;
  wrong[1].moleculeCount = 5;
  WriteFile(dir / kLibraryIndexName, IndexText(wrong));
  CHECK_THROWS(LoadPacketLibrary(dir, kPacketCompatibilityTag), "molecule count");
  wrong = entries;
  wrong[0].energyDepositEv = 9990.0;
  wrong[0].escapedEnergyEv = 10.0;
  wrong[0].energyResidualEv = 0.0;
  WriteFile(dir / kLibraryIndexName, IndexText(wrong));
  CHECK_THROWS(LoadPacketLibrary(dir, kPacketCompatibilityTag), "energy deposit");

  // A missing packet file is rejected.
  WriteFile(dir / kLibraryIndexName, IndexText(entries));
  std::filesystem::remove(dir / PacketFileName(1));
  CHECK_THROWS(LoadPacketLibrary(dir, kPacketCompatibilityTag), "cannot open packet file");

  // A missing index is rejected.
  std::filesystem::remove(dir / kLibraryIndexName);
  CHECK_THROWS(LoadPacketLibrary(dir, kPacketCompatibilityTag), "cannot open");

  std::filesystem::remove_all(dir);
}

void TestEnergyLedgerEnforced()
{
  // An unbalanced packet (energy lost to nowhere) cannot be written.
  auto packet = MakePacket(9000.0, 1);
  packet.escapedEnergyEv = 900.0;  // 100 eV missing
  std::ostringstream out;
  CHECK_THROWS(Write(out, packet), "does not match the primary energy");
}

}  // namespace

int main(int argc, char** argv)
{
  // Scratch directory for the file tests: argv[1] if given, else the current one.
  const std::filesystem::path scratch =
      argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
  std::filesystem::create_directories(scratch);
  TestFileNamesAndEntry();
  TestIndexRoundTrip();
  TestIndexWriteRejects();
  TestIndexReadRejects();
  TestLoadLibrary(scratch);
  TestEnergyLedgerEnforced();
  return 0;
}
