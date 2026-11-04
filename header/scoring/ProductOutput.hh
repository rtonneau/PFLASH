/// \file ProductOutput.hh
/// \brief Write the tables of one Output product in every format chosen for it.
/// Geant4-facing layer: uses G4Exception and OutputDir, and dispatches to the
/// portable TableWriters, Hdf5TableFile and RootTableFile.
/// Call between runs on the master or serial thread, with workers idle.

#ifndef ProductOutput_hh
#define ProductOutput_hh 1

#include "scoring/OutputFormats.hh"
#include "scoring/OutputTable.hh"

#include "globals.hh"

#include <string>
#include <vector>

namespace ProductOutput
{
  /// Writes `tables` in every format of OutputFormats::Effective(product),
  /// through OutputDir::Resolve (which applies the configured directory,
  /// subfolder and prefix).
  ///  - h5 / root: one file `<Stem>.<ext>` holding all tables.
  ///  - csv / txt: one file per table: `<Stem>.<ext>` for the first table,
  ///    `<Stem><TableNameCapitalised>.<ext>` for the others.
  /// Each written name, with `prefix` prepended literally, is appended to
  /// `files`. A write failure is a JustWarning G4Exception naming the file;
  /// the remaining files are still written. Does nothing when `tables` is empty.
  void Write(OutputFormats::Product product, const std::vector<OutputTable> &tables,
             const G4String &prefix, std::vector<std::string> &files);
}

#endif
