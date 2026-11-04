/// \file ReactionCounter.cc
/// \brief Per-event reaction-firing counts in configurable time bins.
/// Geant4 types and the portable EventStat / OutputTable; no kernel services.
/// Bin configuration is locked; callers coordinate shared counter mutation.

#include "scoring/ReactionCounter.hh"

#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <mutex>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
// Coarse decade edges share checkpoints with the denser ScoreSpecies record
// grid; the final edge keeps the same margin below 1 microsecond.
const std::vector<G4double> kDefaultTimeBins = {
    1 * CLHEP::picosecond,     10 * CLHEP::picosecond,  100 * CLHEP::picosecond,
    1000 * CLHEP::picosecond,  10000 * CLHEP::picosecond, 100000 * CLHEP::picosecond,
    999999 * CLHEP::picosecond,
};

std::mutex gTimeBinsMutex;
std::vector<G4double> gTimeBins = kDefaultTimeBins;

// Local, dependency-free time-unit lookup for ParseBinEdgesList().
// Deliberately not G4UnitDefinition::GetValueOf(): that call reaches into
// the precompiled Geant4 DLL and is unsafe from this plain-logic Debug test
// binary (observed to corrupt memory when the installed Geant4 libraries
// are a different build type). Keep this parser independent of kernel services.
// Names/symbols mirror Geant4's Time category.
G4bool TimeUnitValue(const G4String& unit, G4double& valueOut)
{
  static const std::map<G4String, G4double> kTimeUnits = {
      {"picosecond", CLHEP::picosecond},   {"ps", CLHEP::picosecond},
      {"nanosecond", CLHEP::nanosecond},   {"ns", CLHEP::nanosecond},
      {"microsecond", CLHEP::microsecond}, {"us", CLHEP::microsecond},
      {"millisecond", CLHEP::millisecond}, {"ms", CLHEP::millisecond},
      {"second", CLHEP::second},           {"s", CLHEP::second},
  };
  auto it = kTimeUnits.find(unit);
  if (it == kTimeUnits.end()) {
    return false;
  }
  valueOut = it->second;
  return true;
}
}  // namespace

std::vector<G4double> ReactionCounter::DefaultBinEdges()
{
  return kDefaultTimeBins;
}

void ReactionCounter::ConfigureBinEdges(const std::vector<G4double>& edges)
{
  if (edges.empty()) {
    return;
  }
  std::vector<G4double> sorted = edges;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

  std::lock_guard<std::mutex> lock(gTimeBinsMutex);
  gTimeBins = std::move(sorted);
}

G4bool ReactionCounter::ParseBinEdgesList(const G4String& text, std::vector<G4double>& edgesOut,
                                           G4String& error)
{
  std::istringstream stream(text);
  std::vector<G4String> tokens;
  G4String token;
  while (stream >> token) {
    tokens.push_back(token);
  }

  if (tokens.size() < 2) {
    error = "expected at least one edge value followed by a unit, got: '" + text + "'";
    return false;
  }

  const G4String& unit = tokens.back();
  G4double unitValue = 0.;
  if (!TimeUnitValue(unit, unitValue)) {
    error = "'" + unit + "' is not a recognized time unit";
    return false;
  }

  std::vector<G4double> edges;
  edges.reserve(tokens.size() - 1);
  for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
    try {
      std::size_t consumed = 0;
      G4double value = std::stod(tokens[i], &consumed);
      if (consumed != tokens[i].size()) {
        throw std::invalid_argument(tokens[i]);
      }
      edges.push_back(value * unitValue);
    }
    catch (const std::exception&) {
      error = "'" + tokens[i] + "' is not a valid edge value";
      return false;
    }
  }

  edgesOut = std::move(edges);
  return true;
}

G4double ReactionCounter::BinFor(G4double time)
{
  std::lock_guard<std::mutex> lock(gTimeBinsMutex);
  for (G4double edge : gTimeBins) {
    if (time <= edge) {
      return edge;
    }
  }
  return gTimeBins.back();
}

G4double ReactionCounter::LowerEdgeFor(G4double upperEdge)
{
  std::lock_guard<std::mutex> lock(gTimeBinsMutex);
  G4double lower = 0.;
  for (G4double edge : gTimeBins) {
    if (edge < upperEdge) {
      lower = edge;
    }
  }
  return lower;
}

void ReactionCounter::Record(const G4String& reactionLabel, G4double time)
{
  ++fCurrent[BinFor(time)][reactionLabel];
}

void ReactionCounter::CommitEvent()
{
  for (const auto& [bin, reactions] : fCurrent) {
    for (const auto& [label, count] : reactions) {
      fStats[bin][label].Add(static_cast<double>(count));
    }
  }
  fCurrent.clear();
  ++fCommittedEvents;
}

void ReactionCounter::DiscardEvent()
{
  fCurrent.clear();
}

void ReactionCounter::Merge(const ReactionCounter& other)
{
  // Copy first so a self-merge doubles instead of iterating while inserting.
  const Stats otherStats = other.fStats;
  const std::int64_t otherEvents = other.fCommittedEvents;
  for (const auto& [bin, reactions] : otherStats) {
    for (const auto& [label, stat] : reactions) {
      fStats[bin][label].Merge(stat);
    }
  }
  fCommittedEvents += otherEvents;
}

void ReactionCounter::Clear()
{
  fCurrent.clear();
  fStats.clear();
  fCommittedEvents = 0;
}

std::map<G4String, G4int> ReactionCounter::BuildReactionIdMap() const
{
  std::map<G4String, G4int> idMap;
  for (const auto& [bin, reactions] : fStats) {
    for (const auto& entry : reactions) {
      idMap.emplace(entry.first, 0);
    }
  }
  G4int nextId = 0;
  for (auto& [label, id] : idMap) {
    id = nextId++;
  }
  return idMap;
}

std::vector<OutputTable> ReactionCounter::ToTables() const
{
  using CT = OutputTable::ColumnType;
  const std::map<G4String, G4int> idMap = BuildReactionIdMap();

  OutputTable reactions("reactions",
                        "Reactions: per-event firing counts of bimolecular reactions per time bin");
  reactions.AddColumn("reactionId", "", "Reaction id; see the reaction catalogue", CT::Int64);
  reactions.AddColumn("binStart_ns", "ns", "Lower edge of the time bin (0 for the first bin)",
                      CT::Double);
  reactions.AddColumn("binEnd_ns", "ns", "Upper edge of the time bin", CT::Double);
  reactions.AddColumn("nEvents", "", "Events in the dump (zeros included)", CT::Int64);
  reactions.AddColumn("countSum", "", "Sum over events of the firing count", CT::Double);
  reactions.AddColumn("countSumSq", "", "Sum over events of the squared firing count", CT::Double);
  reactions.AddColumn("countMean", "", "Mean firing count per event", CT::Double);
  reactions.AddColumn("countStdErr", "", "Standard error of the mean firing count per event",
                      CT::Double);
  for (const auto& [bin, perReaction] : fStats) {
    const double binStart_ns = LowerEdgeFor(bin) / CLHEP::nanosecond;
    const double binEnd_ns = bin / CLHEP::nanosecond;
    for (const auto& [label, stat] : perReaction) {
      reactions.AddRow({static_cast<std::int64_t>(idMap.at(label)), binStart_ns, binEnd_ns,
                        fCommittedEvents, stat.Sum(), stat.SumSq(), stat.Mean(fCommittedEvents),
                        stat.StdErr(fCommittedEvents)});
    }
  }

  OutputTable catalog("reactionCatalog", "ReactionsCatalog: reaction id to reaction label");
  catalog.AddColumn("reactionId", "", "Reaction id used by the reactions table", CT::Int64);
  catalog.AddColumn("reaction", "", "Reaction label (A + B -> C + D)", CT::String);
  for (const auto& [label, id] : idMap) {
    catalog.AddRow({static_cast<std::int64_t>(id), std::string(label)});
  }

  return {std::move(reactions), std::move(catalog)};
}
