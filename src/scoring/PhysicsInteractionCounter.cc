/// \file PhysicsInteractionCounter.cc
/// \brief Per-event counts of caller-supplied physical-interaction labels.
/// Standard library and Geant4 types; no kernel services.
/// Callers coordinate access to shared mutable counters.

#include "scoring/PhysicsInteractionCounter.hh"

void PhysicsInteractionCounter::Record(const G4String& label)
{
  ++fCurrent[label];
}

void PhysicsInteractionCounter::CommitEvent()
{
  for (const auto& [label, count] : fCurrent) {
    fStats[label].Add(static_cast<double>(count));
  }
  fCurrent.clear();
  ++fCommittedEvents;
}

void PhysicsInteractionCounter::DiscardEvent()
{
  fCurrent.clear();
}

void PhysicsInteractionCounter::Merge(const PhysicsInteractionCounter& other)
{
  // Copy first so a self-merge doubles instead of iterating while inserting.
  const Stats otherStats = other.fStats;
  const std::int64_t otherEvents = other.fCommittedEvents;
  for (const auto& [label, stat] : otherStats) {
    fStats[label].Merge(stat);
  }
  fCommittedEvents += otherEvents;
}

void PhysicsInteractionCounter::Clear()
{
  fCurrent.clear();
  fStats.clear();
  fCommittedEvents = 0;
}

OutputTable PhysicsInteractionCounter::ToTable() const
{
  using CT = OutputTable::ColumnType;
  OutputTable table("physicsInteractions",
                    "PhysicsInteractions: per-event counts of discrete G4DNA physics processes");
  table.AddColumn("process", "", "Physics process name", CT::String);
  table.AddColumn("nEvents", "", "Events in the dump (zeros included)", CT::Int64);
  table.AddColumn("countSum", "", "Sum over events of the process count", CT::Double);
  table.AddColumn("countSumSq", "", "Sum over events of the squared process count", CT::Double);
  table.AddColumn("countMean", "", "Mean count per event", CT::Double);
  table.AddColumn("countStdErr", "", "Standard error of the mean count per event", CT::Double);
  for (const auto& [label, stat] : fStats) {
    table.AddRow({std::string(label), fCommittedEvents, stat.Sum(), stat.SumSq(),
                  stat.Mean(fCommittedEvents), stat.StdErr(fCommittedEvents)});
  }
  return table;
}
