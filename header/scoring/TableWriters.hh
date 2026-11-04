/// \file TableWriters.hh
/// \brief csv and txt serialisers for OutputTable. Standard library only.
///
/// Self-contained, project-agnostic unit: copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy).

#ifndef TableWriters_h
#define TableWriters_h 1

#include "scoring/OutputTable.hh"

#include <iosfwd>
#include <string>

namespace TableWriters
{
  /// Header row of column names, then one line per row. Doubles round-trip
  /// (shortest exact form); strings are quoted (with "" for an embedded
  /// quote) only when they contain a comma or a double quote. Strings are
  /// otherwise unpadded.
  void WriteCsv(std::ostream &out, const OutputTable &table);

  /// "# <productDescription>" line (productDescription is written verbatim,
  /// e.g. "Species: mean molecule count per event"), "# units: col=unit ..."
  /// line (unitless columns omitted), a column-name row, then rows aligned
  /// with spaces; doubles keep 6 significant digits. Strings are padded to
  /// the widest value; an empty string is written as "-", and a string
  /// containing whitespace or a double quote is double-quoted (with "" for
  /// an embedded quote). pandas.read_csv(sep=r'\s+', comment='#') reads it back.
  void WriteTxt(std::ostream &out, const OutputTable &table,
                const std::string &productDescription);
}

#endif
