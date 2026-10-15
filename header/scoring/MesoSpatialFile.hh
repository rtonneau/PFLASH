/// \file MesoSpatialFile.hh
/// \brief HDF5 writer for the per-event spatial snapshots of the mesoscopic stage.
///
/// Portable and project-agnostic: standard library and HDF5 C++ only, no Geant4,
/// no dependency on other dnachem-min classes (adjust the include path prefix on
/// copy). Each event appends its groups to one staged file
/// (<outdir>/.pending_meso_spatial/SpeciesMesoSpatial.h5); at dump time
/// MoveStaged() moves it to a target chosen by the caller.
///
/// Layout: /run<R>/event<E>/snapshot<k>/ with attributes time_ns and cellSize_nm
/// and datasets position_nm (N x 3 float64) and counts (N x S uint32), sparse:
/// only cells holding at least one molecule are listed (N may be 0). Records
/// that share a mesh state share the datasets through HDF5 hard links.

#ifndef MesoSpatialFile_hh
#define MesoSpatialFile_hh 1

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MesoSpatialFile
{
  /// One mesh state.
  struct Snapshot
  {
    double cellSize_nm = 0.;
    std::vector<double> position_nm;   ///< N*3, row-major, cell centres
    std::vector<std::uint32_t> counts; ///< N*S, row-major, column order = species
  };

  /// One record time; snapshot is an index into EventData::snapshots.
  struct Record
  {
    double time_ns;
    std::size_t snapshot;
  };

  struct EventData
  {
    int runId = 0;
    int eventId = 0;
    std::vector<Snapshot> snapshots; ///< distinct mesh states
    std::vector<Record> records;     ///< one per record time, ascending
  };

  constexpr int kFormatVersion = 2;

  /// Path of the format description, written as the root attribute formatDoc.
  constexpr const char* kFormatDoc = "docs/output/SpeciesMesoSpatial-h5.md";

  /// "SpeciesMesoSpatial.h5"
  std::string FileName();

  /// outputDir + "/.pending_meso_spatial", or ".pending_meso_spatial" when outputDir is empty.
  std::string StagingDir(const std::string& outputDir);

  /// StagingDir(outputDir) + "/" + FileName().
  std::string StagedPath(const std::string& outputDir);

  /// Appends one event to the file at path, creating the folder and the file
  /// (with its root attributes) when missing. Returns false with err set, and
  /// leaves the file unchanged, when the file has a different species list, the
  /// event already exists, or the data is inconsistent. Thread-safe.
  bool AppendEvent(const std::string& path, const std::vector<std::string>& species,
                   const EventData& event, std::string& err);

  /// Moves the staged file to target (an existing target is removed first). No
  /// staged file: returns true with moved = false.
  bool MoveStaged(const std::string& outputDir, const std::string& target, bool& moved,
                  std::string& err);
}

#endif
