/// \file PreChemicalRecorder.hh
/// \brief Records the pre-chemical species of one event as an OutputTable.
/// Dependencies: G4VPhysChemIO (Geant4 header only, no kernel service), OutputTable,
/// OutputFormats, TableWriters; no other dnachem-min class.
/// Thread safety: one recorder per event and thread. The process-wide staged table
/// (AppendStaged / TakeStaged) is guarded by a mutex; call TakeStaged between runs.
///
/// Replaces G4PhysChemIO::FormattedText. One row per H2O ionisation/excitation or
/// solvated electron, columns: run, event, parentTrackId, molecule (H2O or e_aq),
/// modification (ionisation, excitation, dissociativeAttachment, solvation), level (electronic level, -1 for
/// e_aq), energy_eV, parentX/Y/Z_nm, x/y/z_nm. Like FormattedText, the energy of an
/// e_aq is the kinetic energy of the incoming track, its parent position is the track
/// position and x/y/z the final position of the solvated electron. For an H2O, x/y/z
/// equal the parent position; an e_aq without final position also gets the parent position.
///
/// Staging: txt/csv write one PreChemical_run<R>_event<E>.<ext> per event (EndEvent);
/// h5/root cannot be appended per event (Hdf5TableFile and RootTableFile write whole
/// files), so EndEvent adds the event's rows to one process-wide table under a mutex
/// and the Dump writes PreChemical.h5/.root from it (TakeStaged) into the staging
/// folder, from where PreChemicalFiles::MoveStaged publishes it.

#ifndef PreChemicalRecorder_hh
#define PreChemicalRecorder_hh 1

#include "scoring/OutputFormats.hh"
#include "scoring/OutputTable.hh"

#include "G4VPhysChemIO.hh"

#include <string>
#include <vector>

class PreChemicalRecorder : public G4VPhysChemIO
{
public:
  PreChemicalRecorder(int runId, int eventId);
  ~PreChemicalRecorder() override = default;

  /// The (empty) table with the PreChemical schema.
  static OutputTable MakeTable();

  /// Row of a water molecule: modification 0 = ionisation, 1 = excitation, 2 = dissociativeAttachment
  /// (anything else is stored as "unknown").
  void AddWaterMolecule(int parentTrackId, int modification, int level, double energy_eV,
                        double x_nm, double y_nm, double z_nm);

  /// Row of a solvated electron; when hasFinalPosition is false the final
  /// position is the parent position.
  void AddSolvatedElectron(int parentTrackId, double energy_eV, double parentX_nm,
                           double parentY_nm, double parentZ_nm, bool hasFinalPosition,
                           double x_nm, double y_nm, double z_nm);

  const OutputTable &Table() const { return table; }

  /// Writes this event's rows, once. For txt and csv: StagedFileName files in
  /// stagingDir (an empty table when nothing was recorded). For h5 and root:
  /// appends the rows to the process-wide staged table. Returns false and fills
  /// err when a file cannot be written; the other formats are still handled.
  bool EndEvent(const std::string &stagingDir, const std::vector<OutputFormats::Format> &formats,
                std::string &err);

  /// Appends all rows of `rows` (same schema) to the process-wide staged table.
  static void AppendStaged(const OutputTable &rows);
  /// Returns the staged table and empties it.
  static OutputTable TakeStaged();
  /// Number of rows currently staged.
  static std::size_t StagedRowCount();

  // G4VPhysChemIO
  void InitializeFile() override {}
  void NewRun() override {}
  void NewEvent() override {}
  void CreateWaterMolecule(G4int electronicModif, G4int electronicLevel, G4double energy,
                           const G4Track *theIncomingTrack) override;
  void CreateSolvatedElectron(const G4Track *theIncomingTrack,
                              G4ThreeVector *finalPosition = nullptr) override;
  void WriteInto(const G4String &, std::ios_base::openmode = std::ios_base::out) override {}
  void CloseFile() override {}

private:
  int run;
  int event;
  bool ended = false;
  OutputTable table;
};

#endif
