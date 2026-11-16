// CaptureLibrary: the packet library written by `capture` (Geant4-free).
//
// A library is one directory holding
//   library.csv          the index, one row per packet, in packet-index order
//   packet-00000.txt ... one PrecursorPacket file per primary (PrecursorPacket.hh)
//   capture.json         a human-readable summary written by `capture`; a
//                        doseCutoff history reads back only the capture box and
//                        the skipped empty events (ReadCaptureSummary)
//
// library.csv columns (camelCase, unit suffix; reals with 17 significant digits,
// so they read back to the same double):
//   packetIndex,file,physicsSeed,primaryParticle,primaryEnergyEv,energyDepositEv,
//   escapedEnergyEv,energyResidualEv,moleculeCount
// energyResidualEv = energyDepositEv + escapedEnergyEv - primaryEnergyEv, the
// per-packet energy ledger (its bound is enforced by PrecursorPacket::Validate).
//
// LoadPacketLibrary reads the index and every packet it names, and rejects any
// mismatch between the two, so a reader always works on a consistent library.
#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "PrecursorPacket.hh"

namespace drs
{

inline constexpr const char kLibraryIndexName[] = "library.csv";

struct LibraryEntry
{
  std::size_t packetIndex = 0;  // 0,1,2... equal to the row number
  std::string file;             // plain file name inside the library directory
  long long physicsSeed = 0;    // seed of the event that produced the packet, >= 1
  std::string primaryParticle;
  double primaryEnergyEv = 0;
  double energyDepositEv = 0;
  double escapedEnergyEv = 0;
  double energyResidualEv = 0;  // deposit + escaped - primary
  std::size_t moleculeCount = 0;
  bool operator==(const LibraryEntry&) const = default;
};

// "packet-00007.txt" for index 7 (at least five digits).
std::string PacketFileName(std::size_t packetIndex);

// Index row describing `packet` (the residual is computed here).
LibraryEntry MakeLibraryEntry(std::size_t packetIndex, long long physicsSeed,
                              const PrecursorPacket& packet);

// Throws std::runtime_error on an entry that cannot be written faithfully
// (index != row, unsafe file name, seed < 1, non-finite energy, a particle
// name with a comma/quote/whitespace).
void WriteLibraryIndex(std::ostream& out, const std::vector<LibraryEntry>& entries);

// Throws std::runtime_error (with the line number) on a wrong header, a wrong
// field count, a malformed number, index != row number, a duplicate or unsafe
// file name, seed < 1, a residual that does not match the energies, or an
// empty index.
std::vector<LibraryEntry> ReadLibraryIndex(std::istream& in);

struct PacketLibrary
{
  std::vector<LibraryEntry> entries;
  std::vector<PrecursorPacket> packets;  // packets[i] belongs to entries[i]; empty until loaded
};

// Reads <directory>/library.csv only; packets get one empty slot per entry.
PacketLibrary LoadLibraryIndex(const std::filesystem::path& directory);

// Reads and checks (as LoadPacketLibrary) the packets with the given indices into
// their slots; the others are left as they are. A history needs only the packets of
// its schedule, which is much faster than reading a large library.
void LoadPackets(PacketLibrary& library, const std::filesystem::path& directory,
                 const std::vector<std::size_t>& indices, const std::string& expectedTag,
                 const PacketLimits& limits = {});

// Reads <directory>/library.csv and every packet (Read with expectedTag and
// limits), then requires each packet to match its row exactly (particle,
// energies, molecule count). Throws std::runtime_error naming the file.
PacketLibrary LoadPacketLibrary(const std::filesystem::path& directory,
                                const std::string& expectedTag, const PacketLimits& limits = {});

struct LibraryTotals
{
  std::size_t packetCount = 0;
  std::size_t moleculeCount = 0;
  double primaryEnergyEv = 0;
  double energyDepositEv = 0;
  double escapedEnergyEv = 0;
  double maxAbsResidualEv = 0;  // largest |energyResidualEv| over the packets
};

LibraryTotals SumLibrary(const std::vector<LibraryEntry>& entries);

// ---- capture.json, the part a doseCutoff history needs ----------------------
inline constexpr const char kCaptureSummaryName[] = "capture.json";

// A launched primary that left no molecule in the box (it missed the box, or
// crossed a corner without an ionisation): `capture` skips it, makes no packet,
// and lists its seed and box deposit in capture.json.
struct SkippedEmptyEvent
{
  long long physicsSeed = 0;  // >= 1
  double energyDepositEv = 0;  // >= 0
  bool operator==(const SkippedEmptyEvent&) const = default;
};

struct CaptureSummary
{
  double captureHalfSideNm = 0;  // half side of the capture (dose) box, > 0
  std::vector<SkippedEmptyEvent> skippedEmptyEvents;
};

// Reads the capture.json text (format "drs-capture-summary"): captureHalfSide_nm
// and skippedEmptyEvents (each {"physicsSeed": integer >= 1,
// "energyDeposit_eV": number >= 0}). Other members are ignored. Throws
// std::runtime_error on malformed JSON, another format or a missing or invalid
// member.
CaptureSummary ReadCaptureSummary(std::istream& in);

// <directory>/capture.json.
CaptureSummary LoadCaptureSummary(const std::filesystem::path& directory);

}  // namespace drs
