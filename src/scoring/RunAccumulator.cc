/// \file RunAccumulator.cc
/// \brief Implementation of RunAccumulator

#include "scoring/RunAccumulator.hh"

#include <set>

namespace
{
  G4double gAccumulatedEnergy = 0.;
  long gAccumulatedEvents = 0;
  ReactionCounter gAccumulatedReactionCounter;
  PhysicsInteractionCounter gAccumulatedInteractionCounter;
  MesoSpeciesCounter gAccumulatedMesoSpeciesCounter;
  TrackLengthTable gAccumulatedTrackLengthTable;
  std::vector<DataNode> gRunEntries;
  G4bool gHasPendingData = false;
  std::set<G4String> gUsedPrefixes;
  std::set<G4String> gUsedSubdirs;
}

void RunAccumulator::Accumulate(G4double energy, long events, const ReactionCounter &reactions,
                                 const PhysicsInteractionCounter &interactions,
                                 const MesoSpeciesCounter &mesoSpecies,
                                 const TrackLengthTable &trackLengths)
{
  gAccumulatedEnergy += energy;
  gAccumulatedEvents += events;
  gAccumulatedReactionCounter.Merge(reactions);
  gAccumulatedInteractionCounter.Merge(interactions);
  gAccumulatedMesoSpeciesCounter.Merge(mesoSpecies);
  gAccumulatedTrackLengthTable.Merge(trackLengths);
  gHasPendingData = true;
}

G4bool RunAccumulator::HasPendingData()
{
  return gHasPendingData;
}

G4double RunAccumulator::GetAccumulatedEnergy()
{
  return gAccumulatedEnergy;
}

long RunAccumulator::GetAccumulatedEvents()
{
  return gAccumulatedEvents;
}

const ReactionCounter &RunAccumulator::GetAccumulatedReactionCounter()
{
  return gAccumulatedReactionCounter;
}

const PhysicsInteractionCounter &RunAccumulator::GetAccumulatedInteractionCounter()
{
  return gAccumulatedInteractionCounter;
}

const MesoSpeciesCounter &RunAccumulator::GetAccumulatedMesoSpeciesCounter()
{
  return gAccumulatedMesoSpeciesCounter;
}

const TrackLengthTable &RunAccumulator::GetAccumulatedTrackLengthTable()
{
  return gAccumulatedTrackLengthTable;
}

void RunAccumulator::AddRunEntry(const DataNode &entry)
{
  gRunEntries.push_back(entry);
}

const std::vector<DataNode> &RunAccumulator::GetRunEntries()
{
  return gRunEntries;
}

void RunAccumulator::ClearAccumulated()
{
  gAccumulatedEnergy = 0.;
  gAccumulatedEvents = 0;
  gAccumulatedReactionCounter.Clear();
  gAccumulatedInteractionCounter.Clear();
  gAccumulatedMesoSpeciesCounter.Clear();
  gAccumulatedTrackLengthTable.Clear();
  gRunEntries.clear();
  gHasPendingData = false;
}

G4bool RunAccumulator::TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                                         G4String &err)
{
  if (enforceUniqueness && gUsedPrefixes.count(prefix) > 0)
  {
    err = "prefix '" + prefix + "' was already used earlier in this run -- choose a different prefix";
    return false;
  }

  gUsedPrefixes.insert(prefix);
  return true;
}

G4bool RunAccumulator::TryReserveSubdir(const G4String &subdir, G4bool enforceUniqueness,
                                         G4String &err)
{
  if (enforceUniqueness && gUsedSubdirs.count(subdir) > 0)
  {
    err = "subfolder '" + subdir + "' was already used earlier in this run -- choose a different name";
    return false;
  }

  gUsedSubdirs.insert(subdir);
  return true;
}
