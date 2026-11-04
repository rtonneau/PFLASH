/// \file OutputTable.cc
/// \brief Implementation of OutputTable and SpeciesSchema (standard library only).
#include "scoring/OutputTable.hh"

#include "scoring/EventStat.hh"

#include <cassert>
#include <utility>

OutputTable::OutputTable(std::string name, std::string description)
  : tableName(std::move(name)), tableDescription(std::move(description))
{
}

void OutputTable::AddColumn(const std::string &columnName, const std::string &unit,
                            const std::string &columnDescription, ColumnType type)
{
  assert(rows.empty() && "OutputTable: columns must be added before the first row");
  if (!rows.empty())
    return;
  columns.push_back(Column{columnName, unit, columnDescription, type});
}

bool OutputTable::IsValidRow(const Row &row) const
{
  if (row.size() != columns.size())
    return false;
  for (std::size_t i = 0; i < row.size(); ++i)
  {
    switch (columns[i].type)
    {
      case ColumnType::Int64:
        if (!std::holds_alternative<std::int64_t>(row[i]))
          return false;
        break;
      case ColumnType::Double:
        if (!std::holds_alternative<double>(row[i]))
          return false;
        break;
      case ColumnType::String:
        if (!std::holds_alternative<std::string>(row[i]))
          return false;
        break;
    }
  }
  return true;
}

bool OutputTable::AddRow(Row row)
{
  const bool valid = IsValidRow(row);
  assert(valid && "OutputTable: row arity or cell type does not match the columns");
  if (!valid)
    return false;
  rows.push_back(std::move(row));
  return true;
}

OutputTable SpeciesSchema::MakeTable(const std::string &tableName,
                                     const std::string &description)
{
  using T = OutputTable::ColumnType;
  OutputTable table(tableName, description);
  table.AddColumn("time_ns", "ns", "Time since the start of the chemical stage", T::Double);
  table.AddColumn("species", "", "Molecule name", T::String);
  table.AddColumn("nEvents", "", "Number of events the sums run over", T::Int64);
  table.AddColumn("countSum", "", "Sum over events of the molecule count", T::Double);
  table.AddColumn("countSumSq", "", "Sum over events of the squared molecule count", T::Double);
  table.AddColumn("countMean", "", "Mean molecule count per event", T::Double);
  table.AddColumn("countStdErr", "", "Standard error of the mean count", T::Double);
  table.AddColumn("gSum", "molecules/100 eV", "Sum over events of the G-value", T::Double);
  table.AddColumn("gSumSq", "(molecules/100 eV)^2", "Sum over events of the squared G-value",
                  T::Double);
  table.AddColumn("gMean", "molecules/100 eV", "Mean G-value per event", T::Double);
  table.AddColumn("gStdErr", "molecules/100 eV", "Standard error of the mean G-value", T::Double);
  return table;
}

OutputTable SpeciesSchema::Build(const std::string &tableName, const std::string &description,
                                 const std::vector<Row> &rows)
{
  OutputTable table = MakeTable(tableName, description);
  for (const Row &r : rows)
  {
    table.AddRow({r.time_ns, r.species, r.nEvents, r.countSum, r.countSumSq,
                  EventStat::Mean(r.countSum, r.nEvents),
                  EventStat::StdErr(r.countSum, r.countSumSq, r.nEvents), r.gSum, r.gSumSq,
                  EventStat::Mean(r.gSum, r.nEvents),
                  EventStat::StdErr(r.gSum, r.gSumSq, r.nEvents)});
  }
  return table;
}
