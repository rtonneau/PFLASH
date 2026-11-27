// PhaseSpaceReader: opens one Phase-space file (format v1, PhaseSpaceFormat.hh)
// and gives random access to its histories (Geant4-free).
//
// The constructor reads and checks the whole file once: magic, version, block
// sizes, header checksum and values, the exact file size, the history table
// (indices 0..N-1, contiguous record ranges, >= 1 record each, increasing
// event IDs, record total) and the content digest; every record is decoded and
// checked (ValidateRecord). Only the header and the history table stay in
// memory. ReadHistory then reads one history's records from disk on demand.
//
// Errors are std::runtime_error whose message starts with
// "phase-space file '<path>': ".
//
// Thread safety: after construction every member function is const and may be
// called concurrently; ReadHistory serialises its file access with an internal
// mutex (MT stage-2 workers can share one reader). The file must not change
// while it is open: ReadHistory does not re-check the digest.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

#include "PhaseSpaceFormat.hh"

namespace phsp
{

// One Phase-space history: identified by (fileId, entry.historyIndex).
struct PhaseSpaceHistory
{
  std::uint64_t fileId = 0;
  HistoryEntry entry;
  std::vector<PhaseSpaceRecord> records;  // entry.recordCount records, in file order
};

// Stage-2 start times of a history's records (ns): each record's time minus
// the earliest record time of the history, so the history starts at t = 0
// (ReplaySource gives these to the primary vertices). Same order as `records`;
// empty for no record.
std::vector<double> ReplayTimesNs(const std::vector<PhaseSpaceRecord>& records);

class PhaseSpaceReader
{
public:
  explicit PhaseSpaceReader(const std::filesystem::path& path);
  PhaseSpaceReader(const PhaseSpaceReader&) = delete;
  PhaseSpaceReader& operator=(const PhaseSpaceReader&) = delete;

  const std::filesystem::path& Path() const { return fPath; }
  const FileHeader& Header() const { return fHeader; }
  const std::vector<HistoryEntry>& Histories() const { return fHistories; }
  std::uint64_t HistoryCount() const { return fHeader.historiesWritten; }

  // Throws when historyIndex >= HistoryCount() or the read fails.
  PhaseSpaceHistory ReadHistory(std::uint64_t historyIndex) const;

private:
  void Open();

  std::filesystem::path fPath;
  FileHeader fHeader;
  std::vector<HistoryEntry> fHistories;
  mutable std::mutex fMutex;
  mutable std::ifstream fStream;
};

}  // namespace phsp
