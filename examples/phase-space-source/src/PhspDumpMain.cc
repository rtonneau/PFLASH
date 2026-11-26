// phsp-dump: print a Phase-space file as CSV (Geant4-free).
//
//   phsp-dump <file.phsp> [--section all|header|histories|records]
//
// `all` (the default) prints three CSV tables, each after a `# <name>` line and
// separated by a blank line; a single section prints that table only, ready
// for a CSV reader. Reals print in the shortest form that reads back to the
// same double. The file is fully validated first (PhaseSpaceReader).
// Exit status: 0 on success, 1 on a file error, 2 on a usage error.
#include <charconv>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

#include "PhaseSpaceReader.hh"

namespace
{

std::string Real(double value)
{
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
}

void PrintHeader(const phsp::PhaseSpaceReader& reader, std::ostream& out)
{
  const phsp::FileHeader& h = reader.Header();
  out << "field,value\n";
  out << "formatVersion," << h.formatVersion << "\n";
  out << "fileId," << phsp::FormatId(h.fileId) << "\n";
  out << "contentDigest," << phsp::FormatId(h.contentDigest) << "\n";
  out << "stage1Seed," << h.stage1Seed << "\n";
  out << "primariesFired," << h.primariesFired << "\n";
  out << "historiesWritten," << h.historiesWritten << "\n";
  out << "recordsWritten," << h.recordsWritten << "\n";
  out << "latticePitchNm," << Real(h.latticePitchNm) << "\n";
  out << "boxSideNm," << Real(h.boxSideNm) << "\n";
  out << "depthNm," << Real(h.depthNm) << "\n";
}

void PrintHistories(const phsp::PhaseSpaceReader& reader, std::ostream& out)
{
  out << "historyIndex,stage1EventId,boxIx,boxIy,firstRecord,recordCount,inBoxDepositEv\n";
  for (const phsp::HistoryEntry& e : reader.Histories())
    out << e.historyIndex << "," << e.stage1EventId << "," << e.boxIx << "," << e.boxIy << "," << e.firstRecord
        << "," << e.recordCount << "," << Real(e.inBoxDepositEv) << "\n";
}

void PrintRecords(const phsp::PhaseSpaceReader& reader, std::ostream& out)
{
  out << "historyIndex,stage1EventId,boxIx,boxIy,recordIndex,pdg,trackId,kineticEnergyEv,xNm,yNm,zNm,"
         "directionX,directionY,directionZ,timeNs,weight\n";
  for (std::uint64_t i = 0; i < reader.HistoryCount(); ++i)
  {
    const phsp::PhaseSpaceHistory history = reader.ReadHistory(i);
    for (std::size_t r = 0; r < history.records.size(); ++r)
    {
      const phsp::PhaseSpaceRecord& p = history.records[r];
      out << history.entry.historyIndex << "," << history.entry.stage1EventId << "," << history.entry.boxIx << ","
          << history.entry.boxIy << "," << history.entry.firstRecord + r << "," << p.pdg << "," << p.trackId << "," << Real(p.kineticEnergyEv)
          << "," << Real(p.positionNm[0]) << "," << Real(p.positionNm[1]) << "," << Real(p.positionNm[2]) << ","
          << Real(p.direction[0]) << "," << Real(p.direction[1]) << "," << Real(p.direction[2]) << ","
          << Real(p.timeNs) << "," << Real(p.weight) << "\n";
    }
  }
}

int Usage()
{
  std::cerr << "usage: phsp-dump <file.phsp> [--section all|header|histories|records]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv)
{
  std::string file;
  std::string section = "all";
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--section" && i + 1 < argc)
      section = argv[++i];
    else if (!arg.empty() && arg[0] != '-' && file.empty())
      file = arg;
    else
      return Usage();
  }
  if (file.empty() || (section != "all" && section != "header" && section != "histories" && section != "records"))
    return Usage();

  try
  {
    const phsp::PhaseSpaceReader reader(file);
    const bool all = section == "all";
    if (all)
      std::cout << "# header\n";
    if (all || section == "header")
      PrintHeader(reader, std::cout);
    if (all)
      std::cout << "\n# histories\n";
    if (all || section == "histories")
      PrintHistories(reader, std::cout);
    if (all)
      std::cout << "\n# records\n";
    if (all || section == "records")
      PrintRecords(reader, std::cout);
    std::cout.flush();
    if (!std::cout)
      throw std::runtime_error("cannot write the output");
  }
  catch (const std::exception& e)
  {
    std::cerr << "phsp-dump: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
