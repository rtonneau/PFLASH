/// \file MesoSpatialRootRows.hh
/// \brief Expands mesoscopic spatial snapshots into the SpeciesMesoSpatial row table (root output).
/// Dependencies: OutputTable and MesoSpatialFile.hh (types only, no HDF5 call); standard library.
/// Thread safety: ExpandEvent is a pure function. AppendStaged / TakeStaged guard one
/// process-wide table with a mutex; call TakeStaged between runs (at a Dump).
///
/// Portable (adjust the include path prefix on copy). Columns: run, event, snapshot
/// (record index k, as in /run<R>/event<E>/snapshot<k>), time_ns, cellSize_nm,
/// x_nm, y_nm, z_nm (cell centre), species, count. One row per non-empty cell and
/// species with count > 0, for every record of the event. RootTableFile writes whole
/// files, so the rows of all events are collected here and the Dump writes the file.

#ifndef MesoSpatialRootRows_hh
#define MesoSpatialRootRows_hh 1

#include "scoring/MesoSpatialFile.hh"
#include "scoring/OutputTable.hh"

#include <cstddef>
#include <string>
#include <vector>

namespace MesoSpatialRootRows
{
  /// The (empty) table with the SpeciesMesoSpatial row schema.
  OutputTable MakeTable();

  /// Appends the rows of one event to table (same schema as MakeTable). Rows follow
  /// the order records, cells, species. A record whose snapshot index is out of range
  /// is skipped. Returns the number of rows appended.
  std::size_t ExpandEvent(OutputTable &table, const std::vector<std::string> &species,
                          const MesoSpatialFile::EventData &event);

  /// Appends all rows of rows (same schema) to the process-wide staged table.
  void AppendStaged(const OutputTable &rows);
  /// Returns the staged table and empties it.
  OutputTable TakeStaged();
  /// Number of rows currently staged.
  std::size_t StagedRowCount();

  /// "SpeciesMesoSpatial.root"
  std::string FileName();
}

#endif
