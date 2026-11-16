// SnapshotWriter: the particle snapshots of a dose-cutoff history, one HDF5
// file (snapshots.h5). Geant4-free; HDF5 is optional (SnapshotWriterNoHdf5.cc
// is the same interface without it: Available() is false and every other call
// throws).
//
// One frame is the state of the history at one time: the position of every
// live molecule (float32, nm, N x 3) and its species (uint8, an index into the
// species table). Layout (format version 1, read it with scripts/snapshots.py):
//
//   /                        attributes formatVersion (int), units (string)
//   /species                 variable-length UTF-8 strings, the species table
//                            (the index is the species id; stored once)
//   /frames/frame000000/     attribute time_ns (double); frames are numbered
//                            in the order they were added
//     position_nm            float32 [N, 3]
//     species                uint8   [N]
//
// The position and species datasets are chunked, shuffled and gzip compressed
// (deflate level 4) when the HDF5 build has the deflate filter; a frame with
// no molecule has empty (contiguous) datasets.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace drs
{

inline constexpr int kSnapshotFormatVersion = 1;
// The species id is a uint8.
inline constexpr std::size_t kMaxSnapshotSpecies = 255;

struct SnapshotFrame
{
  double timeNs = 0;
  std::vector<float> positionsNm;     // x, y, z of molecule 0, then of molecule 1, ...
  std::vector<std::uint8_t> species;  // one id per molecule
};

struct SnapshotFile
{
  int formatVersion = 0;
  std::vector<std::string> species;
  std::vector<SnapshotFrame> frames;
};

class SnapshotWriter
{
 public:
  // True when this build can write and read snapshot files (HDF5 found).
  static bool Available();
  // True when the HDF5 library has the gzip filter, so the datasets are compressed.
  static bool Compressed();

  // Creates (truncates) the file and writes the species table. Throws
  // std::runtime_error when HDF5 is not available, the file cannot be created,
  // or there are more than kMaxSnapshotSpecies species.
  SnapshotWriter(const std::filesystem::path& file, const std::vector<std::string>& species);
  ~SnapshotWriter();
  SnapshotWriter(const SnapshotWriter&) = delete;
  SnapshotWriter& operator=(const SnapshotWriter&) = delete;

  // Appends a frame. Throws std::runtime_error on inconsistent sizes (positions
  // not 3 per molecule), a species id outside the table, a non-finite time, or
  // an HDF5 error.
  void AddFrame(double timeNs, const std::vector<float>& positionsNm, const std::vector<std::uint8_t>& species);

  std::size_t FrameCount() const;
  // Flushes and closes the file (also done by the destructor, without throwing).
  void Close();

 private:
  struct Impl;
  std::unique_ptr<Impl> fImpl;
};

// Reads a whole snapshot file. Throws std::runtime_error when HDF5 is not
// available or the file is missing or not a snapshot file of this version.
SnapshotFile ReadSnapshotFile(const std::filesystem::path& file);

}  // namespace drs
