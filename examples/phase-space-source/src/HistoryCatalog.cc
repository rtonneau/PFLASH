#include "HistoryCatalog.hh"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace phsp
{

HistoryCatalog::HistoryCatalog(const std::vector<std::filesystem::path>& files)
{
  if (files.empty())
    throw std::runtime_error("history catalog: no phase-space file given");
  for (std::size_t i = 0; i < files.size(); ++i)
  {
    auto reader = std::make_unique<PhaseSpaceReader>(files[i]);
    for (std::size_t j = 0; j < fFiles.size(); ++j)
      if (fFiles[j]->Header().fileId == reader->Header().fileId)
        throw std::runtime_error("history catalog: file ID " + FormatId(reader->Header().fileId) +
                                 " given twice: '" + fFiles[j]->Path().string() + "' (position " +
                                 std::to_string(j) + ") and '" + files[i].string() + "' (position " +
                                 std::to_string(i) + ")");
    fFirstGlobal.push_back(fHistoryCount);
    fHistoryCount += reader->HistoryCount();
    fFiles.push_back(std::move(reader));
  }
}

const PhaseSpaceReader& HistoryCatalog::File(std::size_t ordinal) const
{
  if (ordinal >= fFiles.size())
    throw std::runtime_error("history catalog: file position " + std::to_string(ordinal) + " does not exist (" +
                             std::to_string(fFiles.size()) + " files)");
  return *fFiles[ordinal];
}

std::uint64_t HistoryCatalog::FirstGlobalIndex(std::size_t ordinal) const
{
  File(ordinal);  // range check
  return fFirstGlobal[ordinal];
}

HistoryKey HistoryCatalog::Locate(std::uint64_t globalIndex) const
{
  if (globalIndex >= fHistoryCount)
    throw std::runtime_error("history catalog: global history " + std::to_string(globalIndex) +
                             " is past the end (" + std::to_string(fHistoryCount) + " histories in " +
                             std::to_string(fFiles.size()) + " files)");
  // Last file whose first index is <= globalIndex; with ties (empty files) it
  // is the last of them, the one that holds the history.
  const auto next = std::upper_bound(fFirstGlobal.begin(), fFirstGlobal.end(), globalIndex);
  const std::size_t ordinal = static_cast<std::size_t>(next - fFirstGlobal.begin()) - 1;
  return {ordinal, fFiles[ordinal]->Header().fileId, globalIndex - fFirstGlobal[ordinal]};
}

HistoryRange HistoryCatalog::Select(std::uint64_t first, std::uint64_t count) const
{
  if (first > fHistoryCount || count > fHistoryCount - first)
    throw std::runtime_error("history catalog: range of " + std::to_string(count) + " histories from global history " +
                             std::to_string(first) + " goes past the end (" + std::to_string(fHistoryCount) +
                             " histories in " + std::to_string(fFiles.size()) + " files)");
  return {first, count};
}

PhaseSpaceHistory HistoryCatalog::ReadHistory(std::uint64_t globalIndex) const
{
  const HistoryKey key = Locate(globalIndex);
  return fFiles[key.fileOrdinal]->ReadHistory(key.historyIndex);
}

}  // namespace phsp
