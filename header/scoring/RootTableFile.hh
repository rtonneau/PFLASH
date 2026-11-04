/// \file RootTableFile.hh
/// \brief Write OutputTables as one ROOT file (one TTree ntuple per table).
/// Standard library, OutputTable and Geant4's g4tools (tools::wroot) only.
///
/// Portable: depends only on OutputTable.hh (adjust the include path prefix on
/// copy) and on the g4tools headers shipped with Geant4. It uses
/// tools::wroot::file and tools::wroot::ntuple directly, so it touches no
/// G4AnalysisManager state and needs no Geant4 kernel services.
///
/// Layout: one TTree per table in the file's top directory, tree name = table
/// name, tree title = table description, one branch (one leaf) per column named
/// after the column: double -> double, string -> std::string, int64 -> double.
/// The last one is a g4tools limit: tools::wroot::ntuple has no 64-bit integer
/// leaf (only char, short, int, float, double, string; G4 ROOT output is the
/// same), so an int64 column is stored as a double, exact up to 2^53; a larger
/// value makes Write fail rather than lose digits.
/// The unit is kept in the leaf title, "<name> [<unit>]" (just "<name>" when
/// the unit is empty); tools::wroot::ntuple does not let the branch title be
/// set (it is the tree name), so read the unit from the leaf title
/// (tools::rroot::tree::find_leaf(name)->title()). Compression is zlib level 1
/// with toolx::compress_buffer, as G4RootFileManager sets it up.
///
/// The file is written whole at a Dump: appending to an existing file is not
/// supported (Write overwrites the path). A caller that needs per-event
/// appends (ticket 11's spatial file) must collect the rows, serialise calls
/// under its own mutex and write the tables when the Dump happens.

#ifndef RootTableFile_hh
#define RootTableFile_hh 1

#include "scoring/OutputTable.hh"

#include <string>
#include <vector>

namespace RootTableFile
{
  /// Write all tables to path (overwritten). Returns false and fills err on failure.
  bool Write(const std::string& path, const std::vector<OutputTable>& tables, std::string& err);
}

#endif
