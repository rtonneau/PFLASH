/// \file TrackLengthTable.cc
/// \brief Implementation of the TrackLengthTable class

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
  const std::size_t n = rows.size();
  if (n == 0) return stat;
  double sum = 0.;
  for (const auto& r : rows) sum += static_cast<double>(get(r));
  stat.mean = sum / static_cast<double>(n);
  if (n < 2) return stat;
  double sq = 0.;
  for (const auto& r : rows) {
    const double d = static_cast<double>(get(r)) - stat.mean;
    sq += d * d;
  }
  const double stddev = std::sqrt(sq / static_cast<double>(n - 1));
  stat.sem = stddev / std::sqrt(static_cast<double>(n));
  return stat;
}
}  // namespace

TrackLengthTable::Summary TrackLengthTable::Summarize() const
{
  Summary s;
  s.nEvents = fRows.size();
  s.primaryLength_nm = ColumnStat(fRows, [](const Row& r) { return r.primaryLength_nm; });
  s.primaryEkin0_keV = ColumnStat(fRows, [](const Row& r) { return r.primaryEkin0_keV; });
  s.primaryEkinEnd_keV = ColumnStat(fRows, [](const Row& r) { return r.primaryEkinEnd_keV; });
  s.secondaryFirstGen_nm = ColumnStat(fRows, [](const Row& r) { return r.secondaryFirstGen_nm; });
  s.nSecondaryFirstGen = ColumnStat(fRows, [](const Row& r) { return r.nSecondaryFirstGen; });
  s.secondaryAll_nm = ColumnStat(fRows, [](const Row& r) { return r.secondaryAll_nm; });
  s.nSecondaryAll = ColumnStat(fRows, [](const Row& r) { return r.nSecondaryAll; });
  for (const auto& r : fRows) {
    switch (r.primaryEnd) {
      case End::Stopped:
        ++s.nStopped;
        break;
      case End::Escaped:
        ++s.nEscaped;
        break;
      case End::Killed:
        ++s.nKilled;
        break;
    }
  }
  return s;
}

namespace
{
void WriteRows(std::ostream& out, const std::vector<TrackLengthTable::Row>& rows, char sep)
{
  const auto flags = out.flags();
  const auto precision = out.precision();
  out << std::fixed << std::setprecision(6);
  for (const auto& r : rows) {
    out << r.run << sep << r.event << sep << r.primaryLength_nm << sep << r.primaryEkin0_keV << sep
        << r.primaryEkinEnd_keV << sep << TrackLengthTable::EndToString(r.primaryEnd) << sep
        << r.secondaryFirstGen_nm << sep << r.nSecondaryFirstGen << sep << r.secondaryAll_nm << sep
        << r.nSecondaryAll << "\n";
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
