/// \file PreChemicalRecorder.cc
/// \brief Records the pre-chemical species of one event as an OutputTable.

#include "scoring/PreChemicalRecorder.hh"

#include "scoring/PreChemicalFiles.hh"
#include "scoring/TableWriters.hh"

#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

#include <cstdint>
#include <fstream>
#include <mutex>
#include <utility>

namespace
{
  std::mutex gStagedMutex;
  OutputTable gStaged = PreChemicalRecorder::MakeTable();

  const char *ModificationName(int modification)
  {
    if (modification == 0) return "ionisation";
    if (modification == 1) return "excitation";
    if (modification == 2) return "dissociativeAttachment";
    return "unknown";
  }

  bool WriteStream(const std::string &path, OutputFormats::Format format, const OutputTable &table,
                   std::string &err)
  {
    std::ofstream out(path);
    if (out)
    {
      if (format == OutputFormats::Format::Csv)
        TableWriters::WriteCsv(out, table);
      else
        TableWriters::WriteTxt(out, table, table.Description());
      out.close();
    }
    if (!out)
    {
      err += "Cannot write '" + path + "'; ";
      return false;
    }
    return true;
  }
}

OutputTable PreChemicalRecorder::MakeTable()
{
  using T = OutputTable::ColumnType;
  OutputTable t("preChemical", "PreChemical: species created by the physical stage");
  t.AddColumn("run", "", "run number", T::Int64);
  t.AddColumn("event", "", "event number", T::Int64);
  t.AddColumn("parentTrackId", "", "track ID of the incoming track", T::Int64);
  t.AddColumn("molecule", "", "H2O or e_aq", T::String);
  t.AddColumn("modification", "", "ionisation, excitation, dissociativeAttachment (H2O) or solvation (e_aq)", T::String);
  t.AddColumn("level", "", "electronic level of the H2O, -1 for e_aq", T::Int64);
  t.AddColumn("energy_eV", "eV", "energy deposited (H2O) or kinetic energy (e_aq)", T::Double);
  t.AddColumn("parentX_nm", "nm", "x of the incoming track", T::Double);
  t.AddColumn("parentY_nm", "nm", "y of the incoming track", T::Double);
  t.AddColumn("parentZ_nm", "nm", "z of the incoming track", T::Double);
  t.AddColumn("x_nm", "nm", "x of the species", T::Double);
  t.AddColumn("y_nm", "nm", "y of the species", T::Double);
  t.AddColumn("z_nm", "nm", "z of the species", T::Double);
  return t;
}

PreChemicalRecorder::PreChemicalRecorder(int runId, int eventId)
  : run(runId), event(eventId), table(MakeTable())
{
}

void PreChemicalRecorder::AddWaterMolecule(int parentTrackId, int modification, int level,
                                           double energy_eV, double x_nm, double y_nm, double z_nm)
{
  table.AddRow({std::int64_t(run), std::int64_t(event), std::int64_t(parentTrackId),
                std::string("H2O"), std::string(ModificationName(modification)),
                std::int64_t(level), energy_eV, x_nm, y_nm, z_nm, x_nm, y_nm, z_nm});
}

void PreChemicalRecorder::AddSolvatedElectron(int parentTrackId, double energy_eV,
                                              double parentX_nm, double parentY_nm,
                                              double parentZ_nm, bool hasFinalPosition,
                                              double x_nm, double y_nm, double z_nm)
{
  if (!hasFinalPosition)
  {
    x_nm = parentX_nm;
    y_nm = parentY_nm;
    z_nm = parentZ_nm;
  }
  table.AddRow({std::int64_t(run), std::int64_t(event), std::int64_t(parentTrackId),
                std::string("e_aq"), std::string("solvation"), std::int64_t(-1), energy_eV,
                parentX_nm, parentY_nm, parentZ_nm, x_nm, y_nm, z_nm});
}

void PreChemicalRecorder::CreateWaterMolecule(G4int electronicModif, G4int electronicLevel,
                                              G4double energy, const G4Track *track)
{
  const G4ThreeVector &p = track->GetPosition();
  AddWaterMolecule(track->GetTrackID(), electronicModif, electronicLevel, energy / eV,
                   p.x() / nanometer, p.y() / nanometer, p.z() / nanometer);
}

void PreChemicalRecorder::CreateSolvatedElectron(const G4Track *track, G4ThreeVector *finalPosition)
{
  const G4ThreeVector &p = track->GetPosition();
  const G4ThreeVector f = (finalPosition != nullptr) ? *finalPosition : G4ThreeVector();
  AddSolvatedElectron(track->GetTrackID(), track->GetKineticEnergy() / eV, p.x() / nanometer,
                      p.y() / nanometer, p.z() / nanometer, finalPosition != nullptr,
                      f.x() / nanometer, f.y() / nanometer, f.z() / nanometer);
}

bool PreChemicalRecorder::EndEvent(const std::string &stagingDir,
                                   const std::vector<OutputFormats::Format> &formats,
                                   std::string &err)
{
  if (ended) return true;
  ended = true;
  bool ok = true;
  bool tableFormat = false;
  for (OutputFormats::Format format : formats)
  {
    if (format == OutputFormats::Format::Txt || format == OutputFormats::Format::Csv)
    {
      const std::string path =
          stagingDir + "/" +
          PreChemicalFiles::StagedFileName(run, event, OutputFormats::Extension(format));
      ok = WriteStream(path, format, table, err) && ok;
    }
    else
    {
      tableFormat = true; // h5 and/or root share one staged table
    }
  }
  if (tableFormat) AppendStaged(table);
  return ok;
}

void PreChemicalRecorder::AppendStaged(const OutputTable &rows)
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  for (const OutputTable::Row &row : rows.Rows()) gStaged.AddRow(row);
}

OutputTable PreChemicalRecorder::TakeStaged()
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  OutputTable taken = MakeTable();
  std::swap(taken, gStaged);
  return taken;
}

std::size_t PreChemicalRecorder::StagedRowCount()
{
  std::lock_guard<std::mutex> lock(gStagedMutex);
  return gStaged.Rows().size();
}
