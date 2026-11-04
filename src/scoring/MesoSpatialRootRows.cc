/// \file MesoSpatialRootRows.cc
/// \brief Expands mesoscopic spatial snapshots into the SpeciesMesoSpatial row table.

#include "scoring/MesoSpatialRootRows.hh"

#include <cstdint>
#include <mutex>
#include <utility>

namespace
{
  std::mutex gStagedMutex;
  OutputTable gStaged = MesoSpatialRootRows::MakeTable();
}

OutputTable MesoSpatialRootRows::MakeTable()
{
  using T = OutputTable::ColumnType;
  OutputTable t("speciesMesoSpatial", "SpeciesMesoSpatial: species counts per non-empty mesh cell");
  t.AddColumn("run", "", "run number", T::Int64);
  t.AddColumn("event", "", "event number", T::Int64);
  t.AddColumn("snapshot", "", "record index within the event", T::Int64);
  t.AddColumn("time_ns", "ns", "record time", T::Double);
  t.AddColumn("cellSize_nm", "nm", "mesh cell size", T::Double);
  t.AddColumn("x_nm", "nm", "x of the cell centre", T::Double);
  t.AddColumn("y_nm", "nm", "y of the cell centre", T::Double);
  t.AddColumn("z_nm", "nm", "z of the cell centre", T::Double);
  t.AddColumn("species", "", "species name", T::String);
  t.AddColumn("count", "", "molecules of the species in the cell (> 0)", T::Int64);
  return t;
}

std::size_t MesoSpatialRootRows::ExpandEvent(OutputTable &table,
                                             const std::vector<std::string> &species,
                                             const MesoSpatialFile::EventData &event)
{
  std::size_t added = 0;
  const std::size_t nSpecies = species.size();
  for (std::size_t k = 0; k < event.records.size(); ++k)
  {
    const MesoSpatialFile::Record &record = event.records[k];
    if (record.snapshot >= event.snapshots.size()) continue;
    const MesoSpatialFile::Snapshot &snap = event.snapshots[record.snapshot];
    const std::size_t nCells = snap.position_nm.size() / 3;
    for (std::size_t c = 0; c < nCells; ++c)
    {
      for (std::size_t s = 0; s < nSpecies; ++s)
      {
        const std::size_t index = c * nSpecies + s;
        if (index >= snap.counts.size()) break;
        const std::uint32_t count = snap.counts[index];
        if (count == 0) continue;
        if (table.AddRow({std::int64_t(event.runId), std::int64_t(event.eventId),
                          std::int64_t(k), record.time_ns, snap.cellSize_nm,
                          snap.position_nm[3 * c], snap.position_nm[3 * c + 1],
                          snap.position_nm[3 * c + 2], species[s], std::int64_t(count)}))
          ++added;
      }
    }
  }
  return added;
}

void MesoSpatialRootRows::AppendStaged(const OutputTable &rows)
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  for (const OutputTable::Row &row : rows.Rows()) gStaged.AddRow(row);
}

OutputTable MesoSpatialRootRows::TakeStaged()
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  OutputTable taken = MakeTable();
  std::swap(taken, gStaged);
  return taken;
}

std::size_t MesoSpatialRootRows::StagedRowCount()
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  return gStaged.Rows().size();
}

std::string MesoSpatialRootRows::FileName() { return "SpeciesMesoSpatial.root"; }
