#include "PhaseSpaceReader.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace phsp
{

namespace
{

constexpr std::size_t kRecordsPerChunk = 4096;

bool ReadBytes(std::ifstream& in, std::uint8_t* data, std::size_t size)
{
  in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
  return static_cast<std::size_t>(in.gcount()) == size;
}

}  // namespace

PhaseSpaceReader::PhaseSpaceReader(const std::filesystem::path& path) : fPath(path)
{
  try
  {
    Open();
  }
  catch (const std::exception& e)
  {
    throw std::runtime_error("phase-space file '" + fPath.string() + "': " + e.what());
  }
}

void PhaseSpaceReader::Open()
{
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(fPath, error);
  if (error)
    throw std::runtime_error("cannot read the file size (" + error.message() + ")");
  fStream.open(fPath, std::ios::binary);
  if (!fStream)
    throw std::runtime_error("cannot open for reading");
  if (size < kHeaderSize)
    throw std::runtime_error("truncated: " + std::to_string(size) + " bytes, shorter than the " +
                             std::to_string(kHeaderSize) + "-byte header");

  HeaderBytes headerBytes{};
  if (!ReadBytes(fStream, headerBytes.data(), headerBytes.size()))
    throw std::runtime_error("cannot read the header");
  fHeader = DecodeHeader(headerBytes);
  ValidateHeader(fHeader);

  // Expected size, guarded against counts so large they overflow.
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  const std::uint64_t histories = fHeader.historiesWritten;
  const std::uint64_t records = fHeader.recordsWritten;
  if (histories > (kMax - kHeaderSize) / kHistoryEntrySize ||
      records > (kMax - kHeaderSize - histories * kHistoryEntrySize) / kRecordSize)
    throw std::runtime_error("corrupted header: impossible history/record counts");
  const std::uint64_t expected = kHeaderSize + histories * kHistoryEntrySize + records * kRecordSize;
  if (size < expected)
    throw std::runtime_error("truncated: the header announces " + std::to_string(histories) + " histories and " +
                             std::to_string(records) + " records (" + std::to_string(expected) +
                             " bytes), the file has " + std::to_string(size) + " bytes");
  if (size > expected)
    throw std::runtime_error(std::to_string(size - expected) + " unexpected trailing bytes after " +
                             std::to_string(records) + " records");

  // Structural errors are reported after the digest check: on a corrupted
  // file the digest mismatch is the more useful message.
  Fnv1a64 hash;
  std::optional<std::string> firstError;
  fHistories.reserve(static_cast<std::size_t>(histories));
  std::uint64_t nextRecord = 0;
  for (std::uint64_t i = 0; i < histories; ++i)
  {
    HistoryEntryBytes bytes{};
    if (!ReadBytes(fStream, bytes.data(), bytes.size()))
      throw std::runtime_error("cannot read history-table entry " + std::to_string(i));
    hash.Update(bytes.data(), bytes.size());
    const HistoryEntry entry = DecodeHistoryEntry(bytes);
    if (!firstError)
    {
      const std::string where = "history-table entry " + std::to_string(i) + ": ";
      if (entry.historyIndex != i)
        firstError = where + "historyIndex is " + std::to_string(entry.historyIndex);
      else if (entry.firstRecord != nextRecord)
        firstError = where + "firstRecord is " + std::to_string(entry.firstRecord) + ", expected " +
                     std::to_string(nextRecord);
      else if (entry.recordCount == 0 || entry.recordCount > records - nextRecord)
        firstError = where + "recordCount " + std::to_string(entry.recordCount) + " is 0 or past the records";
      else if (i > 0 && entry.stage1EventId <= fHistories.back().stage1EventId)
        firstError = where + "stage-1 event IDs are not strictly increasing";
      else if (!std::isfinite(entry.inBoxDepositEv) || entry.inBoxDepositEv < 0)
        firstError = where + "inBoxDepositEv is not finite and >= 0";
    }
    nextRecord += entry.recordCount;
    fHistories.push_back(entry);
  }
  if (!firstError && nextRecord != records)
    firstError = "the history table covers " + std::to_string(nextRecord) + " records, the header announces " +
                 std::to_string(records);

  std::vector<std::uint8_t> chunk(kRecordsPerChunk * kRecordSize);
  for (std::uint64_t done = 0; done < records;)
  {
    const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(kRecordsPerChunk, records - done));
    if (!ReadBytes(fStream, chunk.data(), count * kRecordSize))
      throw std::runtime_error("cannot read the records");
    hash.Update(chunk.data(), count * kRecordSize);
    for (std::size_t r = 0; r < count && !firstError; ++r)
    {
      RecordBytes bytes{};
      std::copy_n(chunk.begin() + static_cast<std::ptrdiff_t>(r * kRecordSize), kRecordSize, bytes.begin());
      try
      {
        ValidateRecord(DecodeRecord(bytes));
      }
      catch (const std::exception& e)
      {
        firstError = "record " + std::to_string(done + r) + ": " + e.what();
      }
    }
    done += count;
  }

  if (hash.Value() != fHeader.contentDigest)
    throw std::runtime_error("content digest mismatch: computed " + FormatId(hash.Value()) + ", header has " +
                             FormatId(fHeader.contentDigest) + " (corrupted or modified file)");
  if (firstError)
    throw std::runtime_error(*firstError);
}

PhaseSpaceHistory PhaseSpaceReader::ReadHistory(std::uint64_t historyIndex) const
{
  if (historyIndex >= fHistories.size())
    throw std::runtime_error("phase-space file '" + fPath.string() + "': history " + std::to_string(historyIndex) +
                             " does not exist (" + std::to_string(fHistories.size()) + " histories)");
  PhaseSpaceHistory history;
  history.fileId = fHeader.fileId;
  history.entry = fHistories[static_cast<std::size_t>(historyIndex)];
  const std::size_t count = static_cast<std::size_t>(history.entry.recordCount);
  std::vector<std::uint8_t> buffer(count * kRecordSize);
  const std::uint64_t offset =
      kHeaderSize + fHeader.historiesWritten * kHistoryEntrySize + history.entry.firstRecord * kRecordSize;
  {
    std::lock_guard<std::mutex> lock(fMutex);
    fStream.clear();
    fStream.seekg(static_cast<std::streamoff>(offset));
    if (!fStream || !ReadBytes(fStream, buffer.data(), buffer.size()))
      throw std::runtime_error("phase-space file '" + fPath.string() + "': cannot read the records of history " +
                               std::to_string(historyIndex) + " (file changed or removed?)");
  }
  history.records.reserve(count);
  for (std::size_t r = 0; r < count; ++r)
  {
    RecordBytes bytes{};
    std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(r * kRecordSize), kRecordSize, bytes.begin());
    history.records.push_back(DecodeRecord(bytes));
  }
  return history;
}

}  // namespace phsp
