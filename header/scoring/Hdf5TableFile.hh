/// \file Hdf5TableFile.hh
/// \brief Write OutputTables as one HDF5 file.
/// Standard library, OutputTable and HDF5 C++ only.
///
/// Portable: depends only on OutputTable.hh (adjust the include path prefix on
/// copy). The file is written whole at a Dump (not appended per event).
///
/// Layout: one group per table (/<table name>, attribute description), one 1-D
/// dataset per column named after the column (string attributes unit and
/// description); root attribute formatVersion = 1. int64 -> NATIVE_INT64,
/// double -> NATIVE_DOUBLE, string -> variable-length UTF-8. An empty table
/// writes its group and zero-length datasets.

#ifndef Hdf5TableFile_hh
#define Hdf5TableFile_hh 1

#include "scoring/OutputTable.hh"

#include <string>
#include <vector>

namespace Hdf5TableFile
{
  constexpr int kFormatVersion = 1;

  /// True when this build can write HDF5 (Hdf5TableFile.cc); false in the
  /// no-HDF5 stub (Hdf5TableFileNoHdf5.cc).
  bool Available();

  /// Write all tables to path (overwritten). Returns false and fills err on failure.
  bool Write(const std::string& path, const std::vector<OutputTable>& tables, std::string& err);
}

#endif
