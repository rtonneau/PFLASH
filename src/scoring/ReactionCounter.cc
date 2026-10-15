/// \file ReactionCounter.cc
/// \brief Implementation of the ReactionCounter class

#include "scoring/ReactionCounter.hh"

#include "core/OutputDir.hh"

#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <mutex>
#include <ostream>
#include <sstream>
#include <vector>

namespace
{
// Same fixed checkpoint times ScoreSpecies uses for species snapshots, so
// this counter's time axis lines up with Species.Txt by default.
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
// are a different build type -- see project testing docs on keeping this
// class kernel/DLL-free). Names/symbols mirror Geant4's own Time category.
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

void ReactionCounter::Record(const G4String& reactionLabel, G4double time)
{
  ++fCounts[BinFor(time)][reactionLabel];
}

void ReactionCounter::Merge(const ReactionCounter& other)
{
  for (const auto& [bin, reactions] : other.fCounts) {
    for (const auto& [label, count] : reactions) {
      fCounts[bin][label] += count;
    }
  }
}

void ReactionCounter::Clear()
{
  fCounts.clear();
}

void ReactionCounter::WriteAscii(std::ostream& out) const
{
  for (const auto& [bin, reactions] : fCounts) {
    out << bin << "\n";
    for (const auto& [label, count] : reactions) {
      out << label << "    count = " << count << "\n";
    }
  }
}

std::map<G4String, G4int> ReactionCounter::BuildReactionIdMap() const
{
  std::map<G4String, G4int> idMap;
  for (const auto& [bin, reactions] : fCounts) {
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

void ReactionCounter::WriteCsv(G4VAnalysisManager* analysisManager) const
{
  const std::map<G4String, G4int> idMap = BuildReactionIdMap();

  analysisManager->OpenFile(OutputDir::Resolve("Reactions"));

  G4int ntupleId = analysisManager->CreateNtuple("reactions", "reactions");
  analysisManager->CreateNtupleIColumn(ntupleId, "reactionId");
  analysisManager->CreateNtupleDColumn(ntupleId, "time");
  analysisManager->CreateNtupleIColumn(ntupleId, "count");
  analysisManager->FinishNtuple(ntupleId);

  for (const auto& [bin, reactions] : fCounts) {
    for (const auto& [label, count] : reactions) {
      analysisManager->FillNtupleIColumn(ntupleId, 0, idMap.at(label));
      analysisManager->FillNtupleDColumn(ntupleId, 1, bin);
      analysisManager->FillNtupleIColumn(ntupleId, 2, count);
      analysisManager->AddNtupleRow(ntupleId);
    }
  }

  analysisManager->Write();
  analysisManager->CloseFile();
}

void ReactionCounter::WriteMetadata(std::ostream& out) const
{
  out << "reactionId,reaction\n";
  for (const auto& [label, id] : BuildReactionIdMap()) {
    out << id << "," << label << "\n";
  }
}
