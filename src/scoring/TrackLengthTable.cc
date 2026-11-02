/// \file TrackLengthTable.cc
/// \brief Store and summarize per-event primary and secondary track lengths.
/// Standard library only.
/// Callers coordinate access to shared mutable tables and streams.

#include "scoring/TrackLengthTable.hh"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

const char* TrackLengthTable::EndToString(End end)
{
  switch (end) {
    case End::Stopped:
      return "stopped";
    case End::Escaped:
      return "escaped";
    case End::Killed:
      return "killed";
  }
  return "stopped";
}

void TrackLengthTable::Add(const Row& row)
{
  fRows.push_back(row);
}

void TrackLengthTable::Merge(const TrackLengthTable& other)
{
  fRows.insert(fRows.end(), other.fRows.begin(), other.fRows.end());
}

void TrackLengthTable::Clear()
{
  fRows.clear();
}

std::vector<TrackLengthTable::Row> TrackLengthTable::Sorted() const
{
  std::vector<Row> rows = fRows;
  std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
    return a.run != b.run ? a.run < b.run : a.event < b.event;
  });
  return rows;
}

namespace
{
template<typename Getter>
TrackLengthTable::Stat ColumnStat(const std::vector<TrackLengthTable::Row>& rows, Getter get)
{
  TrackLengthTable::Stat stat;
  const std::size_t rowCount = rows.size();
  if (rowCount == 0) return stat;
  double sum = 0.;
  for (const auto& row : rows) sum += static_cast<double>(get(row));
  stat.mean = sum / static_cast<double>(rowCount);
  if (rowCount < 2) return stat;
  double sumSquaredDeviations = 0.;
  for (const auto& row : rows) {
    const double deviation = static_cast<double>(get(row)) - stat.mean;
    sumSquaredDeviations += deviation * deviation;
  }
  const double stddev = std::sqrt(sumSquaredDeviations / static_cast<double>(rowCount - 1));
  stat.sem = stddev / std::sqrt(static_cast<double>(rowCount));
  return stat;
}
}  // namespace

TrackLengthTable::Summary TrackLengthTable::Summarize() const
{
  Summary summary;
  summary.nEvents = fRows.size();
  summary.primaryLength_nm = ColumnStat(fRows, [](const Row& row) { return row.primaryLength_nm; });
  summary.primaryEkin0_keV = ColumnStat(fRows, [](const Row& row) { return row.primaryEkin0_keV; });
  summary.primaryEkinEnd_keV = ColumnStat(fRows, [](const Row& row) { return row.primaryEkinEnd_keV; });
  summary.secondaryFirstGen_nm = ColumnStat(fRows, [](const Row& row) { return row.secondaryFirstGen_nm; });
  summary.nSecondaryFirstGen = ColumnStat(fRows, [](const Row& row) { return row.nSecondaryFirstGen; });
  summary.secondaryAll_nm = ColumnStat(fRows, [](const Row& row) { return row.secondaryAll_nm; });
  summary.nSecondaryAll = ColumnStat(fRows, [](const Row& row) { return row.nSecondaryAll; });
  for (const auto& row : fRows) {
    switch (row.primaryEnd) {
      case End::Stopped:
        ++summary.nStopped;
        break;
      case End::Escaped:
        ++summary.nEscaped;
        break;
      case End::Killed:
        ++summary.nKilled;
        break;
    }
  }
  return summary;
}

namespace
{
constexpr int kTablePrecision = 6;

void WriteRows(std::ostream& out, const std::vector<TrackLengthTable::Row>& rows, char separator)
{
  const auto flags = out.flags();
  const auto precision = out.precision();
  out << std::fixed << std::setprecision(kTablePrecision);
  for (const auto& row : rows) {
    out << row.run << separator << row.event << separator << row.primaryLength_nm << separator << row.primaryEkin0_keV << separator
        << row.primaryEkinEnd_keV << separator << TrackLengthTable::EndToString(row.primaryEnd) << separator
        << row.secondaryFirstGen_nm << separator << row.nSecondaryFirstGen << separator << row.secondaryAll_nm << separator
        << row.nSecondaryAll << "\n";
  }
  out.flags(flags);
  out.precision(precision);
}
}  // namespace

void TrackLengthTable::WriteCsv(std::ostream& out) const
{
  out << "run,event,primaryLength_nm,primaryEkin0_keV,primaryEkinEnd_keV,primaryEnd,"
         "secondaryFirstGen_nm,nSecondaryFirstGen,secondaryAll_nm,nSecondaryAll\n";
  WriteRows(out, Sorted(), ',');
}

void TrackLengthTable::WriteAscii(std::ostream& out) const
{
  out << "# run event primaryLength_nm primaryEkin0_keV primaryEkinEnd_keV primaryEnd "
         "secondaryFirstGen_nm nSecondaryFirstGen secondaryAll_nm nSecondaryAll\n";
  WriteRows(out, Sorted(), ' ');
}
