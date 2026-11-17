#include "ActivationObserver.hh"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
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
  fActivationTimesNs.clear();
  for (const auto& source : checked) fActivationTimesNs.push_back(source.activationTimeNs);
  std::sort(fActivationTimesNs.begin(), fActivationTimesNs.end());
  fActivationTimesNs.erase(std::unique(fActivationTimesNs.begin(), fActivationTimesNs.end()),
                           fActivationTimesNs.end());
  std::vector<ReactionLog::SourceRange> ranges;
  for (const auto& source : checked)
    ranges.push_back({static_cast<long long>(source.label), static_cast<int>(source.firstTrackId),
                      static_cast<int>(source.lastTrackId)});
  fReactions.Configure(std::move(ranges));
  fCheck.emplace(std::move(checked));
  fSnapshotWriter.reset();
  fSnapshotTimesNs.clear();
  fSnapshotSpecies.clear();
  fFrameTimesNs.clear();
  fWatched.clear();
  fNextFrame = fNextArm = fArmedIntervals = fSnapshotsDropped = fSnapshotsMerged = fSnapshotMolecules = 0;
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

void ActivationObserver::EnableSnapshots(const std::filesystem::path& file, const std::vector<double>& timesNs)
{
  if (!fCheck) throw std::invalid_argument("ActivationObserver: EnableSnapshots before Configure");
  if (timesNs.empty()) throw std::invalid_argument("ActivationObserver: no snapshot time");
  for (std::size_t i = 0; i < timesNs.size(); ++i)
    if (!std::isfinite(timesNs[i]) || !(timesNs[i] > 0) || (i > 0 && !(timesNs[i] > timesNs[i - 1])))
      throw std::invalid_argument("ActivationObserver: snapshot times must be positive and increasing");
  // The species table: the same species as the species tables, sorted by name.
  const auto names = AllSpeciesNames();
  const std::vector<std::string> table(names.begin(), names.end());
  fSnapshotWriter = std::make_unique<SnapshotWriter>(file, table);  // throws on a missing HDF5 or a bad file
  fSnapshotSpecies.clear();
  for (std::size_t i = 0; i < table.size(); ++i) fSnapshotSpecies.emplace(table[i], static_cast<std::uint8_t>(i));
  fSnapshotTimesNs = timesNs;
  fFrameTimesNs.clear();
  fWatched.clear();
  fNextFrame = fNextArm = fArmedIntervals = fSnapshotsDropped = fSnapshotsMerged = fSnapshotMolecules = 0;
}

void ActivationObserver::CloseSnapshots()
{
  if (!fSnapshotWriter) return;
  try
  {
    fSnapshotWriter->Close();
  }
  catch (const std::exception& e)
  {
    if (fFailure.empty()) fFailure = e.what();
  }
}

void ActivationObserver::CaptureFrame(double labelNs, const std::vector<const G4Track*>& live)
{
  std::vector<float> positions;
  std::vector<std::uint8_t> species;
  positions.reserve(3 * live.size());
  species.reserve(live.size());
  for (const auto* track : live)
  {
    if (IsWater(*track)) continue;
    const auto name = GetMolecule(track)->GetMolecularConfiguration()->GetName();
    const auto found = fSnapshotSpecies.find(name);
    if (found == fSnapshotSpecies.end())
    {
      Fail("snapshot: the live species " + name + " is not in the species table");
      return;
    }
    const auto& p = track->GetPosition();
    positions.push_back(static_cast<float>(p.x() / nm));
    positions.push_back(static_cast<float>(p.y() / nm));
    positions.push_back(static_cast<float>(p.z() / nm));
    species.push_back(found->second);
  }
  try
  {
    fSnapshotWriter->AddFrame(labelNs, positions, species);
  }
  catch (const std::exception& e)
  {
    Fail(std::string("snapshot: ") + e.what());
    return;
  }
  fFrameTimesNs.push_back(labelNs);
  fSnapshotMolecules += species.size();
}

// Writes every requested time not yet captured that is <= limitNs (< when
// strict), labelled with the clock time. An empty history has no clock to
// speak of (its tracks are gone, the clock stays behind): the requested time
// labels the frame.
void ActivationObserver::CaptureDue(double limitNs, bool strict, double clockNs,
                                    const std::vector<const G4Track*>& live)
{
  bool first = true;
  while (fFailure.empty() && fNextFrame < fSnapshotTimesNs.size())
  {
    const double requested = fSnapshotTimesNs[fNextFrame];
    if (strict ? !(requested < limitNs) : !(requested <= limitNs)) break;
    // Times closer than the clock can tell apart (the early picoseconds) share one frame.
    if (first) CaptureFrame(live.empty() ? requested : clockNs, live);
    else ++fSnapshotsMerged;
    first = false;
    ++fNextFrame;
  }
}

// Adds the watched times of the next inter-activation interval (see the header).
void ActivationObserver::ArmNextInterval(G4double endTime)
{
  if (!fSnapshotWriter || fArmedIntervals >= fActivationTimesNs.size()) return;
  const std::size_t k = fArmedIntervals++;
  const double low = fActivationTimesNs[k] + kSnapshotGuardNs;
  const double high =
      (k + 1 < fActivationTimesNs.size() ? fActivationTimesNs[k + 1] : endTime / ns) - kSnapshotGuardNs;
  while (fNextArm < fSnapshotTimesNs.size() && fSnapshotTimesNs[fNextArm] <= low) ++fNextArm;  // at the stage boundary
  auto* scheduler = G4Scheduler::Instance();
  while (fNextArm < fSnapshotTimesNs.size() && fSnapshotTimesNs[fNextArm] < high)
  {
    const G4double watched = fSnapshotTimesNs[fNextArm] * ns;
    scheduler->AddWatchedTime(watched);
    fWatched.insert(watched);
    ++fNextArm;
  }
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
  if (fSnapshotWriter)
  {
    // No molecule exists before the first activation: no frame there.
    const double first = fActivationTimesNs.front() - kSnapshotGuardNs;
    while (fNextFrame < fSnapshotTimesNs.size() && fSnapshotTimesNs[fNextFrame] < first)
    {
      ++fNextFrame;
      ++fSnapshotsDropped;
    }
    fNextArm = fNextFrame;
    ArmNextInterval(G4Scheduler::Instance()->GetEndTime());
  }
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
  {
    // Bulk-reaction products have no parent ID: match them to the molecule
    // that vanished in this step (ReactionLog::ObserveStep).
    std::vector<drs::ReactionLog::ObservedMolecule> observed;
    observed.reserve(G4ITTrackHolder::Instance()->GetMainList()->size());
    for (auto* track : *G4ITTrackHolder::Instance()->GetMainList())
      if (IsLive(*track) && !IsWater(*track))  // pre-chemical water dissociates, it is no bulk reaction
      {
        const auto& p = track->GetPosition();
        observed.push_back({track->GetTrackID(), track->GetParentID(), p.x(), p.y(), p.z(),
                            &GetMolecule(track)->GetMolecularConfiguration()->GetName()});
      }
    fReactions.ObserveStep(G4Scheduler::Instance()->GetGlobalTime() / ns, observed);
  }
  // The last step before the next delayed list joins: the clock reached the
  // list's time, or no track is left to step. The next call is then NewStage.
  auto* holder = G4ITTrackHolder::Instance();
  auto* scheduler = G4Scheduler::Instance();
  const G4double next = holder->GetNextTime();  // DBL_MAX when no list is left
  const G4double now = scheduler->GetGlobalTime();
  const bool beforeMerge = next < scheduler->GetEndTime() && (!(now < next) || !holder->MainListsNOTEmpty());
  std::vector<const G4Track*> live;
  if (fSnapshotWriter && fNextFrame < fSnapshotTimesNs.size())
  {
    // A watched time reached (the step was clipped to it), or the last step
    // before a delayed list joins (frames in the guard before the list).
    const double nowNs = now / ns;
    if (beforeMerge)
    {
      live = LiveTracks();
      CaptureDue(next / ns, true, nowNs, live);
    }
    else if (fSnapshotTimesNs[fNextFrame] <= nowNs + 1.0e-9)
    {
      live = LiveTracks();
      CaptureDue(nowNs + 1.0e-9, false, nowNs, live);
    }
    if (!fFailure.empty()) return;
  }
  if (beforeMerge)
  {
    if (live.empty()) live = LiveTracks();
    TakeSnapshot(live);
    if (fFailure.empty()) ArmNextInterval(scheduler->GetEndTime());
    if (fFailure.empty() && fSteadyState) SampleSteadyState(live, now);
  }
}

void ActivationObserver::NewStage()
{
  if (!fFailure.empty()) return;
  const G4double now = G4Scheduler::Instance()->GetGlobalTime();
  if (fSnapshotWriter && now / ns != fCheck->NextActivationTimeNs())
  {
    // A stage G4Scheduler runs at a watched time (a snapshot time inside an
    // interval), not an activation: nothing joined.
    const auto watched = fWatched.lower_bound(now - 1.0e-9 * ns);
    if (watched != fWatched.end() && *watched <= now + 1.0e-9 * ns) return;
  }
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
  // Keep the origin of every live molecule: a bulk-reaction product is
  // resolved through its parent, which may be gone by the time it reacts.
  for (const auto* track : live) fReactions.Learn(track->GetTrackID(), track->GetParentID());
  // No frame here: the pre-chemical molecules of this activation dissociate in
  // the first step, so the snapshot times at (or just after) it are taken at the
  // first UserPostTimeStepAction that follows.
}

void ActivationObserver::UserReactionAction(const G4Track& trackA, const G4Track& trackB,
                                            const std::vector<G4Track*>* products)
{
  if (!fFailure.empty()) return;
  {
    // G4DNAMakeReaction pushed the products (IDs assigned) before this call.
    std::vector<int> productIds;
    std::vector<std::string> productNames;
    if (products != nullptr)
      for (const auto* product : *products)
      {
        productIds.push_back(product->GetTrackID());
        productNames.push_back(GetMolecule(product)->GetMolecularConfiguration()->GetName());
      }
    fReactions.RecordReaction(trackA.GetGlobalTime() / ns, trackA.GetTrackID(), trackA.GetParentID(),
                              GetMolecule(trackA)->GetMolecularConfiguration()->GetName(), trackB.GetTrackID(),
                              trackB.GetParentID(), GetMolecule(trackB)->GetMolecularConfiguration()->GetName(),
                              productIds, productNames);
  }
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
  if (fSnapshotWriter && fNextFrame < fSnapshotTimesNs.size())
  {
    // Times at the end (or after the last live track vanished): the end state.
    const double endNs = G4Scheduler::Instance()->GetEndTime() / ns;
    CaptureDue(endNs + 1.0e-9, false, endNs, LiveTracks());
    if (!fFailure.empty()) return;
  }
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
