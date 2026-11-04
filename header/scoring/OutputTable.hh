/// \file OutputTable.hh
/// \brief Format-neutral typed table for the Output products.
/// Standard library only.
///
/// Self-contained, project-agnostic unit: copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy). The table
/// knows no file format; TableWriters (csv, txt) serialise it. Not thread safe.
///
/// Column names are camelCase with a unit suffix (time_ns); the unit is
/// also stored on the column (ns, nm, eV, keV, molecules/100 eV, empty for
/// unitless) for txt/h5/root metadata.

#ifndef OutputTable_h
#define OutputTable_h 1

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

class OutputTable
{
public:
  enum class ColumnType
  {
    Int64,
    Double,
    String
  };

  /// One cell; its alternative must match the column type
  /// (Int64 -> std::int64_t, Double -> double, String -> std::string).
  using Cell = std::variant<std::int64_t, double, std::string>;
  using Row = std::vector<Cell>;

  struct Column
  {
    std::string name;
    std::string unit;
    std::string description;
    ColumnType type = ColumnType::Double;
  };

  OutputTable(std::string name, std::string description);

  /// Append a column. Columns must be added before the first row; adding one
  /// afterwards is a programming error (assert in Debug, ignored otherwise).
  void AddColumn(const std::string &columnName, const std::string &unit,
                 const std::string &columnDescription, ColumnType type);

  /// True when row has one cell per column and each cell's type matches.
  bool IsValidRow(const Row &row) const;

  /// Append a row. A row of the wrong arity or cell type is rejected: assert
  /// in Debug; in Release it is not stored and false is returned.
  bool AddRow(Row row);

  const std::string &Name() const { return tableName; }
  const std::string &Description() const { return tableDescription; }
  const std::vector<Column> &Columns() const { return columns; }
  const std::vector<Row> &Rows() const { return rows; }

private:
  std::string tableName;
  std::string tableDescription;
  std::vector<Column> columns;
  std::vector<Row> rows;
};

/// The Species / SpeciesMeso table shared by the particle-based and the
/// mesoscopic products, built from plain data so both produce identical
/// tables. Columns, in order: time_ns, species, nEvents, countSum,
/// countSumSq, countMean, countStdErr, gSum, gSumSq, gMean, gStdErr.
/// G-values are in molecules/100 eV.
namespace SpeciesSchema
{
  /// Per-(time, species) sums over nEvents events (zeros count as events).
  struct Row
  {
    double time_ns = 0.0;
    std::string species;
    std::int64_t nEvents = 0;
    double countSum = 0.0;
    double countSumSq = 0.0;
    double gSum = 0.0;
    double gSumSq = 0.0;
  };

  /// Empty table with the schema's columns.
  OutputTable MakeTable(const std::string &tableName, const std::string &description);

  /// Table with the schema's columns and one row per entry; mean and standard
  /// error come from EventStat.
  OutputTable Build(const std::string &tableName, const std::string &description,
                    const std::vector<Row> &rows);
}

#endif
