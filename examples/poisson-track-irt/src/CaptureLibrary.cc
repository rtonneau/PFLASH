#include "CaptureLibrary.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ptirt
{
namespace
{

constexpr const char* kHeader =
    "packetIndex,file,physicsSeed,primaryParticle,primaryEnergyEv,energyDepositEv,"
    "escapedEnergyEv,energyResidualEv,moleculeCount";
constexpr std::size_t kFieldCount = 9;

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error("CaptureLibrary: " + message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

std::string FormatReal(double x)
{
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "%.17g", x);
  return buffer;
}

// A plain file name: no directory part, no "." / "..", no CSV-special or
// whitespace characters.
bool IsPlainFileName(const std::string& name)
{
  if (name.empty() || name.size() > 255 || name == "." || name == "..") return false;
  for (unsigned char c : name)
    if (c <= ' ' || c == 0x7f || c == '/' || c == '\\' || c == ':' || c == ',' || c == '"')
      return false;
  return true;
}

bool IsCsvToken(const std::string& text)
{
  if (text.empty()) return false;
  for (unsigned char c : text)
    if (c <= ' ' || c == 0x7f || c == ',' || c == '"') return false;
  return true;
}

double Residual(const LibraryEntry& e)
{
  return e.energyDepositEv + e.escapedEnergyEv - e.primaryEnergyEv;
}

std::vector<std::string> SplitCsv(const std::string& line)
{
  std::vector<std::string> fields;
  std::string field;
  std::istringstream in(line);
  while (std::getline(in, field, ',')) fields.push_back(field);
  if (!line.empty() && line.back() == ',') fields.emplace_back();
  return fields;
}

double ParseReal(const std::string& token, const std::string& where)
{
  char* end = nullptr;
  const double value = std::strtod(token.c_str(), &end);
  Require(!token.empty() && *end == '\0' && std::isfinite(value),
          where + "malformed or non-finite number '" + token + "'");
  return value;
}

long long ParseInteger(const std::string& token, const std::string& where)
{
  char* end = nullptr;
  const long long value = std::strtoll(token.c_str(), &end, 10);
  Require(!token.empty() && *end == '\0', where + "malformed integer '" + token + "'");
  return value;
}

std::size_t ParseCount(const std::string& token, const std::string& where)
{
  const long long value = ParseInteger(token, where);
  Require(value >= 0, where + "negative count '" + token + "'");
  return static_cast<std::size_t>(value);
}

}  // namespace

std::string PacketFileName(std::size_t packetIndex)
{
  char buffer[48];
  std::snprintf(buffer, sizeof buffer, "packet-%05zu.txt", packetIndex);
  return buffer;
}

LibraryEntry MakeLibraryEntry(std::size_t packetIndex, long long physicsSeed,
                              const PrecursorPacket& packet)
{
  LibraryEntry e;
  e.packetIndex = packetIndex;
  e.file = PacketFileName(packetIndex);
  e.physicsSeed = physicsSeed;
  e.primaryParticle = packet.primaryParticle;
  e.primaryEnergyEv = packet.primaryEnergyEv;
  e.energyDepositEv = packet.energyDepositEv;
  e.escapedEnergyEv = packet.escapedEnergyEv;
  e.energyResidualEv = Residual(e);
  e.moleculeCount = packet.molecules.size();
  return e;
}

void WriteLibraryIndex(std::ostream& out, const std::vector<LibraryEntry>& entries)
{
  out << kHeader << '\n';
  for (std::size_t row = 0; row < entries.size(); ++row)
  {
    const auto& e = entries[row];
    const std::string where = "entry " + std::to_string(row) + ": ";
    Require(e.packetIndex == row, where + "packet index differs from the row number");
    Require(IsPlainFileName(e.file), where + "unsafe packet file name '" + e.file + "'");
    Require(e.physicsSeed >= 1, where + "physics seed must be >= 1");
    Require(IsCsvToken(e.primaryParticle), where + "primary particle must be a plain token");
    for (double x : {e.primaryEnergyEv, e.energyDepositEv, e.escapedEnergyEv, e.energyResidualEv})
      Require(std::isfinite(x), where + "non-finite energy");
    Require(e.energyResidualEv == Residual(e), where + "residual does not match the energies");
    out << e.packetIndex << ',' << e.file << ',' << e.physicsSeed << ',' << e.primaryParticle << ','
        << FormatReal(e.primaryEnergyEv) << ',' << FormatReal(e.energyDepositEv) << ','
        << FormatReal(e.escapedEnergyEv) << ',' << FormatReal(e.energyResidualEv) << ','
        << e.moleculeCount << '\n';
  }
  if (!out) Fail("output stream failed");
}

std::vector<LibraryEntry> ReadLibraryIndex(std::istream& in)
{
  std::string line;
  Require(static_cast<bool>(std::getline(in, line)), "empty index (no header)");
  if (!line.empty() && line.back() == '\r') line.pop_back();
  Require(line == kHeader, "line 1: unexpected header '" + line + "'");

  std::vector<LibraryEntry> entries;
  std::set<std::string> files;
  std::size_t lineNumber = 1;
  while (std::getline(in, line))
  {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string where = "line " + std::to_string(lineNumber) + ": ";
    Require(!line.empty(), where + "blank line");
    const auto f = SplitCsv(line);
    Require(f.size() == kFieldCount, where + "expected " + std::to_string(kFieldCount) +
                                         " fields, found " + std::to_string(f.size()));
    LibraryEntry e;
    e.packetIndex = ParseCount(f[0], where);
    Require(e.packetIndex == entries.size(), where + "packet index " + f[0] +
                                                 " differs from the row number " +
                                                 std::to_string(entries.size()));
    e.file = f[1];
    Require(IsPlainFileName(e.file), where + "unsafe packet file name '" + e.file + "'");
    Require(files.insert(e.file).second, where + "duplicate packet file '" + e.file + "'");
    e.physicsSeed = ParseInteger(f[2], where);
    Require(e.physicsSeed >= 1, where + "physics seed must be >= 1");
    e.primaryParticle = f[3];
    Require(IsCsvToken(e.primaryParticle), where + "primary particle must be a plain token");
    e.primaryEnergyEv = ParseReal(f[4], where);
    e.energyDepositEv = ParseReal(f[5], where);
    e.escapedEnergyEv = ParseReal(f[6], where);
    e.energyResidualEv = ParseReal(f[7], where);
    Require(e.energyResidualEv == Residual(e),
            where + "energyResidualEv does not equal deposit + escaped - primary");
    e.moleculeCount = ParseCount(f[8], where);
    entries.push_back(std::move(e));
  }
  Require(!entries.empty(), "the index lists no packet");
  return entries;
}

PacketLibrary LoadPacketLibrary(const std::filesystem::path& directory,
                                const std::string& expectedTag, const PacketLimits& limits)
{
  const auto indexPath = directory / kLibraryIndexName;
  std::ifstream index(indexPath, std::ios::binary);
  Require(static_cast<bool>(index), "cannot open " + indexPath.string());
  PacketLibrary library;
  try
  {
    library.entries = ReadLibraryIndex(index);
  }
  catch (const std::exception& e)
  {
    Fail(indexPath.string() + ": " + e.what());
  }

  library.packets.reserve(library.entries.size());
  for (const auto& entry : library.entries)
  {
    const auto path = directory / entry.file;
    std::ifstream in(path, std::ios::binary);
    Require(static_cast<bool>(in), "cannot open packet file " + path.string());
    PrecursorPacket packet;
    try
    {
      packet = Read(in, expectedTag, limits);
    }
    catch (const std::exception& e)
    {
      Fail(path.string() + ": " + e.what());
    }
    const std::string where = path.string() + ": packet does not match its library.csv row: ";
    Require(packet.primaryParticle == entry.primaryParticle, where + "primary particle");
    Require(packet.primaryEnergyEv == entry.primaryEnergyEv, where + "primary energy");
    Require(packet.energyDepositEv == entry.energyDepositEv, where + "energy deposit");
    Require(packet.escapedEnergyEv == entry.escapedEnergyEv, where + "escaped energy");
    Require(packet.molecules.size() == entry.moleculeCount, where + "molecule count");
    library.packets.push_back(std::move(packet));
  }
  return library;
}

LibraryTotals SumLibrary(const std::vector<LibraryEntry>& entries)
{
  LibraryTotals t;
  for (const auto& e : entries)
  {
    ++t.packetCount;
    t.moleculeCount += e.moleculeCount;
    t.primaryEnergyEv += e.primaryEnergyEv;
    t.energyDepositEv += e.energyDepositEv;
    t.escapedEnergyEv += e.escapedEnergyEv;
    t.maxAbsResidualEv = std::max(t.maxAbsResidualEv, std::abs(e.energyResidualEv));
  }
  return t;
}

}  // namespace ptirt
