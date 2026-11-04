/// \file MesoSpatialFile.hh
/// \brief Write and publish per-event Mesoscopic stage snapshots.
/// Standard library and HDF5 C++ only.
/// AppendEvent serializes HDF5 access; MoveStaged requires writers to be idle.
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

  /// Caller-owned event with unique IDs in the destination and ordered record times.
  /// Geometry/times must be physically meaningful; AppendEvent only checks shapes/indices.
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

  /// True when this build can write the HDF5 file (MesoSpatialFile.cc); false in
  /// the no-HDF5 stub (MesoSpatialFileNoHdf5.cc).
  bool Available();

  /// "SpeciesMesoSpatial.h5"
  std::string FileName();

  /// outputDir + "/.pending_meso_spatial", or ".pending_meso_spatial" when outputDir is empty.
  std::string StagingDir(const std::string& outputDir);

  /// StagingDir(outputDir) + "/" + FileName().
  std::string StagedPath(const std::string& outputDir);

  /// Appends one event to the file at path, creating the folder and the file
  /// (with its root attributes) when missing. species fixes the ordered columns.
  /// Shape/index validation, an incompatible species list or a duplicate event
  /// return false with err set before event data is written. HDF5/filesystem
  /// failures also return false; HDF5 failure may leave a partial file. Success
  /// leaves err unchanged. Non-HDF5 exceptions propagate. Calls are serialized
  /// internally; callers keep the supplied inputs unchanged during the call.
  bool AppendEvent(const std::string& path, const std::vector<std::string>& species,
                   const EventData& event, std::string& err);

  /// Moves the staged file to target (an existing target is removed first). No
  /// staged file (or an error checking its existence): true with moved = false.
  /// Other filesystem errors: false, moved = false, err set; an old target
  /// may already have been removed. Success: true, moved = true. No rollback.
  /// Requires AppendEvent/writers to be idle; allocation exceptions propagate.
  bool MoveStaged(const std::string& outputDir, const std::string& target, bool& moved,
                  std::string& err);
}

#endif
