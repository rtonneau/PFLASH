// HistoryCatalog: the global numbering of the Phase-space histories of an
// ordered list of Phase-space files (Geant4-free).
//
// Global index = file order, then history index: the histories of file 0 are
// 0..N0-1, those of file 1 are N0..N0+N1-1, and so on. A file with no history
// takes no index. The same list in the same order always gives the same
// numbering, which is what lets a stage-2 run be repeated or split
// (`/phsp/source/first k` + `/run/beamOn N` replays Select(k, N)).
//
// Errors (std::runtime_error): an empty list, a file that does not open or
// validate (PhaseSpaceReader), a file ID given twice (the same file listed
// twice, or a copy of it), and an index or range past the end.
//
// Thread safety: after construction every member function is const and may be
// called concurrently (ReadHistory goes through the file's PhaseSpaceReader).
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "PhaseSpaceReader.hh"

namespace phsp
{

struct HistoryKey
{
  std::size_t fileOrdinal = 0;  // position of the file in the list
  std::uint64_t fileId = 0;
  std::uint64_t historyIndex = 0;  // within that file
  bool operator==(const HistoryKey&) const = default;
};

// Global histories first .. first + count - 1.
struct HistoryRange
{
  std::uint64_t first = 0;
  std::uint64_t count = 0;
  bool operator==(const HistoryRange&) const = default;
};

class HistoryCatalog
{
public:
  explicit HistoryCatalog(const std::vector<std::filesystem::path>& files);

  std::size_t FileCount() const { return fFiles.size(); }
  const PhaseSpaceReader& File(std::size_t ordinal) const;
  // Global index of the first history of a file (equals the next file's when empty).
  std::uint64_t FirstGlobalIndex(std::size_t ordinal) const;
  std::uint64_t HistoryCount() const { return fHistoryCount; }

  // Throws when globalIndex >= HistoryCount().
  HistoryKey Locate(std::uint64_t globalIndex) const;
  // Throws when first + count > HistoryCount() (count 0 at first <= HistoryCount() is allowed).
  HistoryRange Select(std::uint64_t first, std::uint64_t count) const;
  // Locate + PhaseSpaceReader::ReadHistory.
  PhaseSpaceHistory ReadHistory(std::uint64_t globalIndex) const;

private:
  std::vector<std::unique_ptr<PhaseSpaceReader>> fFiles;
  std::vector<std::uint64_t> fFirstGlobal;
  std::uint64_t fHistoryCount = 0;
};

}  // namespace phsp
