#include "ActivationObserver.hh"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#include "DrsPhysicsList.hh"
#include "HistoryBoundedBrownianAction.hh"

#include "G4DNABoundingBox.hh"
#include "G4FakeMolecule.hh"
#include "G4H2O.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeCounter.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

namespace drs
{

namespace
{

// Every species the chemistry defines, water and Geant4's G4FakeMolecule
// placeholder ("None", never a live molecule) excluded: each history lists the
// same species at every record time, zero counts included, so ensemble means
// over histories (a Poisson history may draw no source) need no fill-in.
std::set<std::string> AllSpeciesNames()
{
  std::set<std::string> names;
  for (const auto& [userId, configuration] : G4MolecularConfiguration::GetUserIDTable())
    if (configuration != nullptr && configuration->GetDefinition() != G4H2O::Definition() &&
        configuration->GetDefinition() != G4FakeMolecule::Definition())
      names.insert(configuration->GetName());
  return names;
}

G4bool IsLive(const G4Track& track)
{
  const auto status = track.GetTrackStatus();
  return status != fStopAndKill && status != fKillTrackAndSecondaries;
}

std::vector<const G4Track*> LiveTracks()
{
  std::vector<const G4Track*> live;
  for (auto* track : *G4ITTrackHolder::Instance()->GetMainList())
    if (IsLive(*track)) live.push_back(track);
  return live;
}

// The view of the tracks ActivationObserverCheck works on (times in ns).
std::vector<ActivationObserverCheck::Track> CheckTracks(const std::vector<const G4Track*>& live)
{
  std::vector<ActivationObserverCheck::Track> tracks;
  tracks.reserve(live.size());
  for (const auto* track : live)
  {
    const auto& p = track->GetPosition();
    tracks.push_back(ActivationObserverCheck::Track{track->GetTrackID(), track->GetGlobalTime() / ns,
                                                    GetMolecule(track)->GetMolecularConfiguration(),
                                                    {p.x(), p.y(), p.z()}});
  }
  return tracks;
}

G4bool IsWater(const G4Track& track)
{
  return GetMolecule(track)->GetDefinition() == G4H2O::Definition();
}

}  // namespace

void ActivationObserver::Configure(const std::vector<InjectedSource>& sources,
                                   const std::vector<double>& recordTimesNs, const G4DNABoundingBox* box,
                                   G4int counterId)
{
  std::vector<ActivationObserverCheck::Source> checked;
  checked.reserve(sources.size());
  for (const auto& source : sources)
    checked.push_back(ActivationObserverCheck::Source{source.sourceIndex, source.activationTime / ns,
                                                      source.firstTrackId, source.lastTrackId});
  fCheck.emplace(std::move(checked));
  fRecordTimesNs = recordTimesNs;
  fBox = box;
  fCounterId = counterId;
  fSteadySettings.reset();
  fSteadyState.reset();
  fSpeciesIndex.clear();
  fStopRequested = false;
  fStopTime = 0;
  fEnd = EndChecks{};
  fRows.clear();
  fTable = SpeciesTable{};
  fProductFolds = 0;
  fFailure.clear();
  fModelProblem.clear();
  fFinished = false;
}

void ActivationObserver::EnableSteadyState(const SteadyStateSettings& settings)
{
  static_cast<void>(SteadyStateDetector(settings, 1));  // validates the settings now, not when the chemistry runs
  fSteadySettings = settings;
}

void ActivationObserver::Fail(const std::string& message)
{
  if (fFailure.empty()) fFailure = message;
  // The tracks left are deleted at the end without being removed from the
  // counter; it must not see them.
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  G4Scheduler::Instance()->Stop();
}

void ActivationObserver::TakeSnapshot(const std::vector<const G4Track*>& live)
{
  const auto problem = fCheck->Snapshot(CheckTracks(live));
  if (!problem.empty()) Fail(problem);
}

void ActivationObserver::SampleSteadyState(const std::vector<const G4Track*>& live, G4double now)
{
  std::vector<double> counts(fSpeciesIndex.size(), 0.0);
  for (const auto* track : live)
  {
    if (IsWater(*track)) continue;
    const auto found = fSpeciesIndex.find(GetMolecule(track)->GetMolecularConfiguration()->GetName());
    if (found != fSpeciesIndex.end()) counts[found->second] += 1;
  }
  try
  {
    if (fSteadyState->AddSample(counts))
    {
      // Stop before the next delayed list joins; Finish checks at this time.
      fStopRequested = true;
      fStopTime = now;
      G4Scheduler::Instance()->Stop();
    }
  }
  catch (const std::exception& e)
  {
    Fail(std::string("steady-state sample: ") + e.what());
  }
}

void ActivationObserver::StartProcessing()
{
  if (!fCheck)
  {
    Fail("ActivationObserver was not configured before the chemistry ran");
    return;
  }
  // The model is registered when the chemistry runs, so it is checked here, at
  // the time the first source activates.
  fModelProblem = DescribeIrtSynSchedulerProblem(fCheck->NextActivationTimeNs() * ns);
  if (!fModelProblem.empty())
  {
    Fail(fModelProblem);
    return;
  }
  if (fSteadySettings)
  {
    // One sample entry per species, in the order of the species tables.
    const auto names = AllSpeciesNames();
    for (const auto& name : names) fSpeciesIndex.emplace(name, fSpeciesIndex.size());
    fSteadyState.emplace(*fSteadySettings, names.size());
  }
  TakeSnapshot(LiveTracks());  // the history before its first source: empty
  fProgressStart = fLastProgress = std::chrono::steady_clock::now();
}

void ActivationObserver::ReportProgress(G4bool force)
{
  if (!(fProgressIntervalS > 0)) return;
  const auto now = std::chrono::steady_clock::now();
  if (!force && std::chrono::duration<double>(now - fLastProgress).count() < fProgressIntervalS) return;
  fLastProgress = now;
  auto* scheduler = G4Scheduler::Instance();
  std::size_t molecules = 0;
  for (const auto* track : LiveTracks())
    if (!IsWater(*track)) ++molecules;
  G4cout << "HISTORY_PROGRESS wall_s=" << std::chrono::duration<double>(now - fProgressStart).count()
         << " simTime_ns=" << scheduler->GetGlobalTime() / ns << " endpoint_ns=" << scheduler->GetEndTime() / ns
         << " activated=" << (fCheck->Sources().size() - fCheck->PendingSourceCount()) << "/"
         << fCheck->Sources().size() << " liveMolecules=" << molecules
         << " steps=" << scheduler->GetNbSteps() << G4endl;
}

void ActivationObserver::UserPostTimeStepAction()
{
  if (!fFailure.empty()) return;
  ReportProgress(false);
  // The last step before the next delayed list joins: the clock reached the
  // list's time, or no track is left to step. The next call is then NewStage.
  auto* holder = G4ITTrackHolder::Instance();
  auto* scheduler = G4Scheduler::Instance();
  const G4double next = holder->GetNextTime();  // DBL_MAX when no list is left
  const G4double now = scheduler->GetGlobalTime();
  if (next < scheduler->GetEndTime() && (!(now < next) || !holder->MainListsNOTEmpty()))
  {
    const auto live = LiveTracks();
    TakeSnapshot(live);
    if (fFailure.empty() && fSteadyState) SampleSteadyState(live, now);
  }
}

void ActivationObserver::NewStage()
{
  if (!fFailure.empty()) return;
  const G4double now = G4Scheduler::Instance()->GetGlobalTime();
  const auto live = LiveTracks();
  std::vector<std::size_t> activated;
  const auto problem = fCheck->CheckStage(now / ns, CheckTracks(live), activated);
  if (!problem.empty())
  {
    Fail(problem);
    return;
  }
  // The sources were built with the counter inactive; they count from now on.
  auto* counter = G4MoleculeCounterManager::Instance();
  for (const auto i : activated) counter->AddMolecule(live[i], now);
}

void ActivationObserver::UserReactionAction(const G4Track&, const G4Track&, const std::vector<G4Track*>* products)
{
  if (fBox == nullptr || products == nullptr) return;
  for (auto* product : *products)
  {
    auto position = product->GetPosition();
    if (HistoryBoundedBrownianAction::Fold(position, *fBox))
    {
      product->SetPosition(position);
      ++fProductFolds;
    }
  }
}

void ActivationObserver::EndProcessing()
{
  if (!fFailure.empty()) return;
  ReportProgress(true);
  try
  {
    Finish();
  }
  catch (const std::exception& e)
  {
    Fail(std::string("end-of-history check: ") + e.what());
  }
  // After a steady-state stop the scheduler deletes tracks the counter never
  // saw (the sources not yet activated); the counts are read, switch it off.
  if (fEnd.stoppedEarly) G4MoleculeCounterManager::Instance()->SetIsActive(false);
}

void ActivationObserver::Finish()
{
  auto* scheduler = G4Scheduler::Instance();
  fEnd.steps = scheduler->GetNbSteps();
  fEnd.finalTime = scheduler->GetGlobalTime();
  fEnd.stoppedEarly = fStopRequested;
  // After a steady-state stop the checks hold at the stop time.
  const G4double endTime = fStopRequested ? fStopTime : scheduler->GetEndTime();
  fEnd.checkTime = endTime;

  const auto live = LiveTracks();
  const auto checkTracks = CheckTracks(live);
  const auto activation = fStopRequested
                              ? fCheck->CheckEarlyEnd(endTime / ns, checkTracks, fEnd.pendingSourceMolecules)
                              : fCheck->CheckEnd(checkTracks);
  if (!activation.empty())
  {
    Fail(activation);
    return;
  }

  // Every live track is at the end time, inside the box, and the live
  // molecules per configuration are what the counter holds at the end time.
  const G4double tolerance = 64 * std::numeric_limits<double>::epsilon() * std::max(endTime, 1 * ps);
  fEnd.allLiveAtEndTime = true;
  fEnd.allInsideBox = true;
  std::map<const G4MolecularConfiguration*, G4int> liveCount;
  for (const auto* track : live)
  {
    // A molecule of a source not yet activated (steady-state stop) is not in the history.
    if (fStopRequested && track->GetTrackID() >= 1 && !fCheck->IsActivatedMolecule(track->GetTrackID())) continue;
    if (std::abs(track->GetGlobalTime() - endTime) > tolerance) fEnd.allLiveAtEndTime = false;
    if (fBox != nullptr && !fBox->contains(track->GetPosition())) fEnd.allInsideBox = false;
    if (IsWater(*track))
    {
      ++fEnd.liveWater;
      continue;
    }
    ++fEnd.liveMolecules;
    ++liveCount[GetMolecule(track)->GetMolecularConfiguration()];
  }

  const auto* counter =
      G4MoleculeCounterManager::Instance()->GetMoleculeCounter<G4MoleculeCounter>(fCounterId);
  if (counter == nullptr)
  {
    Fail("the history's G4MoleculeCounter (id " + std::to_string(fCounterId) + ") is not registered");
    return;
  }
  fEnd.counterMatchesLive = true;
  const auto recorded = counter->GetRecordedMolecules();
  for (const auto* configuration : recorded)
  {
    ++fEnd.counterSpecies;
    const auto counted = counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(configuration), endTime);
    const auto found = liveCount.find(configuration);
    if (counted != (found != liveCount.end() ? found->second : 0)) fEnd.counterMatchesLive = false;
  }
  for (const auto& [configuration, count] : liveCount)
    if (recorded.count(configuration) == 0) fEnd.counterMatchesLive = false;

  std::ostringstream problem;
  if (!fEnd.allLiveAtEndTime) problem << "a live molecule is not at the end time; ";
  if (!fEnd.allInsideBox) problem << "a live molecule lies outside the reflecting box; ";
  if (!fEnd.counterMatchesLive) problem << "the molecule counter at the end time differs from the live molecules; ";
  if (!problem.str().empty())
  {
    auto text = problem.str();
    Fail("end of history: " + text.substr(0, text.size() - 2));
    return;
  }

  // Species counts on the record grid, summed per species name. After a stop,
  // only the record times at least one counter precision before it.
  std::vector<double> measuredNs;
  for (const auto timeNs : fRecordTimesNs)
    if (!fStopRequested || endTime / ns - timeNs >= kCounterPrecisionNs) measuredNs.push_back(timeNs);
  std::map<std::pair<double, std::string>, double> counts;
  const auto allNames = AllSpeciesNames();
  for (const auto timeNs : measuredNs)
    for (const auto& name : allNames) counts[{timeNs, name}] = 0;
  for (const auto timeNs : measuredNs)
    for (const auto* configuration : recorded)
      counts[{timeNs, configuration->GetName()}] +=
          counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(configuration), timeNs * ns);
  fRows.clear();
  for (const auto& [key, count] : counts)
  {
    SpeciesRow row;
    row.timeNs = key.first;
    row.species = key.second;
    row.count = count;
    fRows.push_back(row);
  }
  fTable = SpeciesTable{};
  fTable.species.assign(allNames.begin(), allNames.end());
  for (const auto timeNs : measuredNs)
  {
    auto& column = fTable.countsAt[timeNs];
    for (const auto& name : fTable.species) column.push_back(counts.at({timeNs, name}));
  }
  fFinished = true;
}

void ActivationObserver::FinishEmpty()
{
  // A history with no source: nothing ran, every species is 0 at every record time.
  fRows.clear();
  for (const auto timeNs : fRecordTimesNs)
    for (const auto& name : AllSpeciesNames())
    {
      SpeciesRow row;
      row.timeNs = timeNs;
      row.species = name;
      fRows.push_back(row);
    }
  fFinished = true;
}

}  // namespace drs
