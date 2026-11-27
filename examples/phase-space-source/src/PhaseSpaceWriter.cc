#include "PhaseSpaceWriter.hh"

#include <climits>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "JsonWriter.hh"

namespace phsp
{

namespace
{

std::runtime_error WriteError(const std::filesystem::path& path, const std::string& message)
{
  return std::runtime_error("phase-space writer '" + path.string() + "': " + message);
}

// Integer when it fits a JSON/DataNode integer, else its decimal string.
DataNode Unsigned(std::uint64_t value)
{
  if (value <= static_cast<std::uint64_t>(LLONG_MAX))
    return DataNode(static_cast<long long>(value));
  return DataNode(std::to_string(value));
}

// Checks the histories, returns the content digest (history table, then
// records, as encoded) and the record count.
std::uint64_t DigestAndCheck(const std::vector<RecordedHistory>& histories, bool singleBox,
                             std::uint64_t& recordCount, double& totalDepositEv)
{
  Fnv1a64 hash;
  recordCount = 0;
  totalDepositEv = 0;
  for (std::size_t i = 0; i < histories.size(); ++i)
  {
    const RecordedHistory& history = histories[i];
    const std::string where = "history " + std::to_string(i) + " (stage-1 event " +
                              std::to_string(history.stage1EventId) + ", box " + std::to_string(history.boxIx) +
                              ", " + std::to_string(history.boxIy) + ")";
    if (i > 0 && HistoryOrderKey(history.stage1EventId, history.boxIx, history.boxIy) <=
                     HistoryOrderKey(histories[i - 1].stage1EventId, histories[i - 1].boxIx, histories[i - 1].boxIy))
      throw std::runtime_error(where +
                               ": histories must be in strictly increasing (stage-1 event ID, box iy, box ix) order");
    if (singleBox && (history.boxIx != 0 || history.boxIy != 0))
      throw std::runtime_error(where + ": with a single box (pitch 0) the box must be (0, 0)");
    if (history.records.empty())
      throw std::runtime_error(where + " has no record (count such primaries in primariesFired only)");
    if (!std::isfinite(history.inBoxDepositEv) || history.inBoxDepositEv < 0)
      throw std::runtime_error(where + ": inBoxDepositEv must be finite and >= 0");
    for (std::size_t r = 0; r < history.records.size(); ++r)
    {
      try
      {
        ValidateRecord(history.records[r]);
      }
      catch (const std::exception& e)
      {
        throw std::runtime_error(where + ", record " + std::to_string(r) + ": " + e.what());
      }
    }
    const HistoryEntry entry{i,         history.stage1EventId,  recordCount, history.records.size(),
                             history.inBoxDepositEv, history.boxIx, history.boxIy};
    const HistoryEntryBytes bytes = EncodeHistoryEntry(entry);
    hash.Update(bytes.data(), bytes.size());
    recordCount += history.records.size();
    totalDepositEv += history.inBoxDepositEv;
  }
  for (const RecordedHistory& history : histories)
    for (const PhaseSpaceRecord& record : history.records)
    {
      const RecordBytes bytes = EncodeRecord(record);
      hash.Update(bytes.data(), bytes.size());
    }
  return hash.Value();
}

void Write(std::ofstream& out, const std::uint8_t* data, std::size_t size)
{
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

std::filesystem::path TempPath(const std::filesystem::path& path)
{
  std::filesystem::path temp = path;
  temp += ".tmp";
  return temp;
}

void CommitTemp(const std::filesystem::path& temp, const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::rename(temp, path, error);
  if (error)
  {
    std::filesystem::remove(temp, error);
    throw WriteError(path, "cannot move the temporary file into place");
  }
}

DataNode MakeSidecar(const WrittenFile& written, const DataNode& stage1Info)
{
  const FileHeader& h = written.header;
  DataNode root = DataNode::MakeObject();
  root.Add("formatVersion", static_cast<long long>(h.formatVersion));
  root.Add("phspFile", written.phspPath.filename().string());
  root.Add("fileId", FormatId(h.fileId));
  root.Add("contentDigest", FormatId(h.contentDigest));
  root.Add("digestAlgorithm", "FNV-1a 64 over the history table and the records, in file order");
  root.Add("stage1Seed", Unsigned(h.stage1Seed));
  root.Add("primariesFired", Unsigned(h.primariesFired));
  root.Add("historiesWritten", Unsigned(h.historiesWritten));
  root.Add("recordsWritten", Unsigned(h.recordsWritten));
  root.Add("latticePitch_nm", h.latticePitchNm);
  root.Add("boxSide_nm", h.boxSideNm);
  root.Add("depth_nm", h.depthNm);
  root.Add("totalInBoxDeposit_eV", written.totalInBoxDepositEv);
  root.Add("fileSize_bytes", Unsigned(written.fileSizeBytes));
  root.Add("recordSize_bytes", static_cast<long long>(kRecordSize));
  if (stage1Info.GetKind() == DataNode::Kind::Object)
    root.Add("stage1", stage1Info);
  return root;
}

}  // namespace

std::filesystem::path SidecarPath(const std::filesystem::path& phspPath)
{
  std::filesystem::path sidecar = phspPath;
  sidecar.replace_extension(".json");
  return sidecar;
}

WrittenFile WritePhaseSpaceFile(const std::filesystem::path& phspPath, const FileDescription& description,
                                const std::vector<RecordedHistory>& histories, const DataNode& stage1Info)
{
  if (phspPath.extension() != ".phsp")
    throw WriteError(phspPath, "the file name must end in .phsp");
  if (stage1Info.GetKind() != DataNode::Kind::Null && stage1Info.GetKind() != DataNode::Kind::Object)
    throw WriteError(phspPath, "stage1Info must be an object");

  WrittenFile written;
  written.phspPath = phspPath;
  written.sidecarPath = SidecarPath(phspPath);
  FileHeader& header = written.header;
  try
  {
    header.contentDigest = DigestAndCheck(histories, description.latticePitchNm == 0, header.recordsWritten,
                                          written.totalInBoxDepositEv);
    header.stage1Seed = description.stage1Seed;
    header.primariesFired = description.primariesFired;
    header.historiesWritten = histories.size();
    header.latticePitchNm = description.latticePitchNm;
    header.boxSideNm = description.boxSideNm;
    header.depthNm = description.depthNm;
    header.fileId = DeriveFileId(header.stage1Seed, header.contentDigest);
    ValidateHeader(header);
  }
  catch (const std::exception& e)
  {
    throw WriteError(phspPath, e.what());
  }
  written.fileSizeBytes = kHeaderSize + header.historiesWritten * kHistoryEntrySize +
                          header.recordsWritten * kRecordSize;

  const std::filesystem::path temp = TempPath(phspPath);
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out)
      throw WriteError(phspPath, "cannot open '" + temp.string() + "' for writing");
    const HeaderBytes headerBytes = EncodeHeader(header);
    Write(out, headerBytes.data(), headerBytes.size());
    std::uint64_t firstRecord = 0;
    for (std::size_t i = 0; i < histories.size(); ++i)
    {
      const HistoryEntry entry{i,
                               histories[i].stage1EventId,
                               firstRecord,
                               histories[i].records.size(),
                               histories[i].inBoxDepositEv,
                               histories[i].boxIx,
                               histories[i].boxIy};
      const HistoryEntryBytes bytes = EncodeHistoryEntry(entry);
      Write(out, bytes.data(), bytes.size());
      firstRecord += entry.recordCount;
    }
    for (const RecordedHistory& history : histories)
      for (const PhaseSpaceRecord& record : history.records)
      {
        const RecordBytes bytes = EncodeRecord(record);
        Write(out, bytes.data(), bytes.size());
      }
    out.close();
    if (!out)
    {
      std::error_code ignored;
      std::filesystem::remove(temp, ignored);
      throw WriteError(phspPath, "write failed");
    }
  }
  // Write both temporaries before moving either into place, and remove an old
  // sidecar first, so a failure never leaves a new .phsp beside a stale .json.
  const std::filesystem::path sidecarTemp = TempPath(written.sidecarPath);
  {
    std::ofstream out(sidecarTemp, std::ios::trunc);
    if (!out)
    {
      std::error_code ignored;
      std::filesystem::remove(temp, ignored);
      throw WriteError(phspPath, "cannot open '" + sidecarTemp.string() + "' for writing");
    }
    JsonWriter::Write(out, MakeSidecar(written, stage1Info));
    out.close();
    if (!out)
    {
      std::error_code ignored;
      std::filesystem::remove(sidecarTemp, ignored);
      std::filesystem::remove(temp, ignored);
      throw WriteError(phspPath, "sidecar write failed");
    }
  }
  {
    std::error_code ec;
    std::filesystem::remove(written.sidecarPath, ec);
    if (ec)
    {
      std::error_code ignored;
      std::filesystem::remove(sidecarTemp, ignored);
      std::filesystem::remove(temp, ignored);
      throw WriteError(phspPath, "cannot remove old sidecar '" + written.sidecarPath.string() +
                                   "': " + ec.message());
    }
  }
  CommitTemp(temp, phspPath);
  CommitTemp(sidecarTemp, written.sidecarPath);
  return written;
}

}  // namespace phsp
