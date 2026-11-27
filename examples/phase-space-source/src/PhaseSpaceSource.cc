#include "PhaseSpaceSource.hh"

#include <algorithm>
#include <cmath>
#include <exception>
#include <sstream>

#include "HistorySeed.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

namespace phsp
{

namespace
{

G4bool SameLength(G4double a, G4double b)
{
  return std::abs(a - b) <= kBoxSideRelativeTolerance * std::max(std::abs(a), std::abs(b));
}

std::string Micrometres(G4double length)
{
  std::ostringstream text;
  text.precision(12);
  text << length / um << " um";
  return text.str();
}

}  // namespace

// ---- ReplaySource ---------------------------------------------------------------

void ReplaySource::AddFile(const std::filesystem::path& path)
{
  if (IsOpen())
    G4Exception("ReplaySource::AddFile", "PhspReplay001", FatalException,
                "/phsp/source/addFile after the files were opened (/run/initialize): the numbering is fixed");
  fFiles.push_back(path);
}

void ReplaySource::Open()
{
  if (IsOpen()) return;
  if (fFiles.empty())
    G4Exception("ReplaySource::Open", "PhspReplay002", FatalException,
                "no phase-space file: give at least one /phsp/source/addFile <path> before /run/initialize");
  try
  {
    fCatalog = std::make_unique<HistoryCatalog>(fFiles);
  }
  catch (const std::exception& e)
  {
    G4Exception("ReplaySource::Open", "PhspReplay003", FatalException, e.what());
    return;
  }
  // Every file must have the box side of the first one.
  const PhaseSpaceReader& first = fCatalog->File(0);
  fBoxSide = first.Header().boxSideNm * nm;
  for (std::size_t i = 1; i < fCatalog->FileCount(); ++i)
  {
    const PhaseSpaceReader& file = fCatalog->File(i);
    const G4double side = file.Header().boxSideNm * nm;
    if (!SameLength(side, fBoxSide))
    {
      const G4String message = "phase-space file '" + file.Path().string() + "' (position " + std::to_string(i) +
                               ") has box side " + Micrometres(side) + ", but '" + first.Path().string() +
                               "' has " + Micrometres(fBoxSide) + ": every file of a replay must share the box side";
      G4Exception("ReplaySource::Open", "PhspReplay004", FatalException, message.c_str());
    }
  }
  if (HasRequestedBoxSide() && !SameLength(fRequestedBoxSide, fBoxSide))
  {
    const G4String message = "/phsp/geom/boxSide " + Micrometres(fRequestedBoxSide) +
                             " differs from the box side of the phase-space files (" + Micrometres(fBoxSide) +
                             "): the stage-2 box must be the stage-1 scoring box";
    G4Exception("ReplaySource::Open", "PhspReplay005", FatalException, message.c_str());
  }
}

const HistoryCatalog& ReplaySource::Catalog() const
{
  if (!IsOpen())
    G4Exception("ReplaySource::Catalog", "PhspReplay006", FatalException, "the phase-space files are not open");
  return *fCatalog;
}

void ReplaySource::BeginRun(std::uint64_t events)
{
  const HistoryCatalog& catalog = Catalog();
  HistoryRange range;
  try
  {
    range = catalog.Select(fFirst, events);
  }
  catch (const std::exception& e)
  {
    const G4String message = std::string(e.what()) + " (/phsp/source/first " + std::to_string(fFirst) +
                             ", /run/beamOn " + std::to_string(events) + ")";
    G4Exception("ReplaySource::BeginRun", "PhspReplay007", FatalException, message.c_str());
    return;
  }
  for (const HistoryRange& done : fRuns)
    if (range.first < done.first + done.count && done.first < range.first + range.count)
    {
      const G4String message = "global histories " + std::to_string(range.first) + ".." +
                               std::to_string(range.first + range.count - 1) + " overlap histories " +
                               std::to_string(done.first) + ".." + std::to_string(done.first + done.count - 1) +
                               " already replayed in this process: Histories.csv holds one row per history";
      G4Exception("ReplaySource::BeginRun", "PhspReplay008", FatalException, message.c_str());
    }
  fRuns.push_back(range);
  fRunFirst.store(range.first);
}

// ---- Event information --------------------------------------------------------------

void ReplayEventInformation::Print() const
{
  G4cout << "phase-space history " << globalIndex << " (file " << FormatId(key.fileId) << ", index "
         << key.historyIndex << ", stage-1 event " << entry.stage1EventId << ", box " << entry.boxIx << ","
         << entry.boxIy << ", " << entry.recordCount << " records)" << G4endl;
}

// ---- Primary generator -----------------------------------------------------------------

G4ParticleDefinition* ParticleOfPdg(G4int pdg)
{
  G4ParticleDefinition* particle = G4ParticleTable::GetParticleTable()->FindParticle(pdg);
  if (particle == nullptr && pdg > 1000000000) particle = G4IonTable::GetIonTable()->GetIon(pdg);
  if (particle == nullptr)
  {
    const G4String message = "no Geant4 particle for PDG code " + std::to_string(pdg);
    G4Exception("PhaseSpaceSource", "PhspReplay009", FatalException, message.c_str());
  }
  return particle;
}

void PhaseSpaceSource::GeneratePrimaries(G4Event* event)
{
  const std::uint64_t globalIndex = fSource.RunFirst() + static_cast<std::uint64_t>(event->GetEventID());
  const HistoryCatalog& catalog = fSource.Catalog();
  PhaseSpaceHistory history;
  HistoryKey key;
  try
  {
    key = catalog.Locate(globalIndex);
    history = catalog.ReadHistory(globalIndex);
  }
  catch (const std::exception& e)
  {
    G4Exception("PhaseSpaceSource::GeneratePrimaries", "PhspReplay010", FatalException, e.what());
    return;
  }

  // ADR 0015: the physical stage of a history depends only on (base seed, file
  // ID, history index). G4WorkerRunManager::GenerateEvent has already seeded
  // the event from the master's list; this replaces it.
  fSeeds = HistoryEngineSeeds(fSource.BaseSeed(), key.fileId, key.historyIndex);
  G4Random::setTheSeeds(fSeeds.data());

  // The history starts at t = 0: each record's time minus the earliest one.
  const std::vector<double> timesNs = ReplayTimesNs(history.records);
  for (std::size_t r = 0; r < history.records.size(); ++r)
  {
    const PhaseSpaceRecord& record = history.records[r];
    const G4ThreeVector position(record.positionNm[0] * nm, record.positionNm[1] * nm, record.positionNm[2] * nm);
    auto* vertex = new G4PrimaryVertex(position, timesNs[r] * ns);
    auto* primary = new G4PrimaryParticle(ParticleOfPdg(record.pdg));
    primary->SetKineticEnergy(record.kineticEnergyEv * eV);
    primary->SetMomentumDirection(G4ThreeVector(record.direction[0], record.direction[1], record.direction[2]));
    primary->SetWeight(record.weight);
    vertex->SetPrimary(primary);
    event->AddPrimaryVertex(vertex);
  }

  auto* information = new ReplayEventInformation();
  information->globalIndex = globalIndex;
  information->key = key;
  information->entry = history.entry;
  event->SetUserInformation(information);
}

}  // namespace phsp
