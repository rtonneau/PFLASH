/// \file MesoSpeciesCounter.cc
/// \brief Implementation of the MesoSpeciesCounter class

#include "scoring/MesoSpeciesCounter.hh"

#include <ostream>

namespace
{
/// Enough digits to keep log-grid times (10 per decade) distinct.
constexpr int kPrecision = 10;
}  // namespace

void MesoSpeciesCounter::Add(double time, const std::string& species, long count)
{
  fCounts[time][species] += count;
}

void MesoSpeciesCounter::Merge(const MesoSpeciesCounter& other)
{
  for (const auto& [time, bySpecies] : other.fCounts) {
    for (const auto& [species, count] : bySpecies) {
      fCounts[time][species] += count;
    }
  }
}

void MesoSpeciesCounter::Clear()
{
  fCounts.clear();
}

bool MesoSpeciesCounter::Empty() const
{
  return fCounts.empty();
}

void MesoSpeciesCounter::WriteAscii(std::ostream& out, long events) const
{
  const auto oldPrecision = out.precision(kPrecision);
  out << "Time is in ns; mean count per event over " << events << " events\n";
  for (const auto& [time, bySpecies] : fCounts) {
    out << time << "\n";
    for (const auto& [species, count] : bySpecies) {
      const double mean = events > 0 ? static_cast<double>(count) / events : 0.;
      out << species << " " << mean << "\n";
    }
  }
  out.precision(oldPrecision);
}

void MesoSpeciesCounter::WriteCsv(std::ostream& out) const
{
  const auto oldPrecision = out.precision(kPrecision);
  out << "time_ns,species,count\n";
  for (const auto& [time, bySpecies] : fCounts) {
    for (const auto& [species, count] : bySpecies) {
      out << time << "," << species << "," << count << "\n";
    }
  }
  out.precision(oldPrecision);
}
