// PhaseSpaceWriter: writes one Phase-space file, `<name>.phsp` (format v1,
// PhaseSpaceFormat.hh) plus its `<name>.json` sidecar (Geant4-free).
//
// Stage 1 collects its histories (from every thread), sorts them by stage-1
// event ID and calls WritePhaseSpaceFile once. Primaries with no record are
// not passed as histories; they count only in `primariesFired`.
//
// Both files are written to `<file>.tmp` first and renamed into place, so an
// interrupted write never leaves a file that looks complete. An existing file
// of the same name is replaced.
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "DataNode.hh"
#include "PhaseSpaceFormat.hh"

namespace phsp
{

// All records of one stage-1 primary, in the order they were recorded.
struct RecordedHistory
{
  std::uint64_t stage1EventId = 0;
  double inBoxDepositEv = 0;  // >= 0
  std::vector<PhaseSpaceRecord> records;  // >= 1 record
};

// What the writer cannot compute from the histories.
struct FileDescription
{
  std::uint64_t stage1Seed = 0;
  std::uint64_t primariesFired = 0;  // >= number of histories
  double latticePitchNm = 0;         // 0 = single box; else >= boxSideNm
  double boxSideNm = 0;
  double depthNm = 0;
};

struct WrittenFile
{
  FileHeader header;  // as written, with fileId and contentDigest
  std::filesystem::path phspPath;
  std::filesystem::path sidecarPath;
  std::uint64_t fileSizeBytes = 0;
  double totalInBoxDepositEv = 0;
};

// `<dir>/<stem>.json` for `<dir>/<stem>.phsp`.
std::filesystem::path SidecarPath(const std::filesystem::path& phspPath);

// Writes the file pair. `stage1Info` (an Object or Null) is copied under the
// sidecar key "stage1" (beam, physics, geometry details chosen by stage 1).
// Throws std::runtime_error, before anything is written, when the path does
// not end in `.phsp`, the histories are not in strictly increasing event ID
// order, a history has no record or an invalid deposit, a record is invalid
// (ValidateRecord), or the description is inconsistent (ValidateHeader); and
// on an I/O failure.
WrittenFile WritePhaseSpaceFile(const std::filesystem::path& phspPath, const FileDescription& description,
                                const std::vector<RecordedHistory>& histories,
                                const DataNode& stage1Info = DataNode());

}  // namespace phsp
