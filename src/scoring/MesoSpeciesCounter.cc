/// \file MesoSpeciesCounter.cc
/// \brief Per-event molecule counts at Mesoscopic stage record times.
/// Standard library and project-portable EventStat/OutputTable only.
/// Callers coordinate access to shared mutable counters.

#include "scoring/MesoSpeciesCounter.hh"

namespace
{
/// G-value reference energy: molecules per 100 eV.
constexpr double kGReferenceEnergy_eV = 100.;
}  // namespace

void MesoSpeciesCounter::Add(double time_ns, const std::string& species, long count)
{
  fCurrent[time_ns][species] += count;
}

void MesoSpeciesCounter::CommitEvent(double edep_eV)
{
  for (const auto& [time_ns, bySpecies] : fCurrent) {
    for (const auto& [species, count] : bySpecies) {
      Stat& stat = fStats[time_ns][species];
      stat.count.Add(static_cast<double>(count));
      stat.g.Add(edep_eV > 0. ? static_cast<double>(count) / edep_eV * kGReferenceEnergy_eV : 0.);
    }
  }
  fCurrent.clear();
  ++fCommittedEvents;
}

void MesoSpeciesCounter::DiscardEvent()
{
  fCurrent.clear();
}

void MesoSpeciesCounter::Merge(const MesoSpeciesCounter& other)
{
  // Copy first so a self-merge doubles instead of iterating while inserting.
  const Stats otherStats = other.fStats;
  const std::int64_t otherEvents = other.fCommittedEvents;
  for (const auto& [time_ns, bySpecies] : otherStats) {
    for (const auto& [species, stat] : bySpecies) {
      Stat& mine = fStats[time_ns][species];
      mine.count.Merge(stat.count);
      mine.g.Merge(stat.g);
    }
  }
  fCommittedEvents += otherEvents;
}

void MesoSpeciesCounter::Clear()
{
  fCurrent.clear();
  fStats.clear();
  fCommittedEvents = 0;
}

bool MesoSpeciesCounter::Empty() const
{
  return fStats.empty();
}

OutputTable MesoSpeciesCounter::ToTable() const
{
  OutputTable table = SpeciesSchema::MakeTable(
      "speciesMeso", "SpeciesMeso: per-event molecule counts and G-values, Mesoscopic stage");
  for (const auto& [time_ns, bySpecies] : fStats) {
    for (const auto& [species, stat] : bySpecies) {
      table.AddRow({time_ns, species, fCommittedEvents, stat.count.Sum(), stat.count.SumSq(),
                    stat.count.Mean(fCommittedEvents), stat.count.StdErr(fCommittedEvents),
                    stat.g.Sum(), stat.g.SumSq(), stat.g.Mean(fCommittedEvents),
                    stat.g.StdErr(fCommittedEvents)});
    }
  }
  return table;
}
