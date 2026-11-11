#include "ActivationObserver.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#include "ContactCount.hh"
#include "ExamplePhysicsList.hh"
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

namespace ptirt
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
  fEnd = EndChecks{};
  fRows.clear();
  fProductFolds = 0;
  fStartZeroStepReactions = 0;
  fContacts = NewOldContacts{};
  fFailure.clear();
  fModelProblem.clear();
  fFinished = false;
}

void ActivationObserver::Fail(const std::string& message)
{
  if (fFailure.empty()) fFailure = message;
  // The tracks left are deleted at the end without being removed from the
  // counter; it must not see them.
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  G4Scheduler::Instance()->Stop();
}

void ActivationObserver::TakeSnapshot()
{
  const auto problem = fCheck->Snapshot(CheckTracks(LiveTracks()));
  if (!problem.empty()) Fail(problem);
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
  TakeSnapshot();  // the history before its first source: empty
}

void ActivationObserver::UserPostTimeStepAction()
{
  if (!fFailure.empty()) return;
  // The last step before the next delayed list joins: the clock reached the
  // list's time, or no track is left to step. The next call is then NewStage.
  auto* holder = G4ITTrackHolder::Instance();
  auto* scheduler = G4Scheduler::Instance();
  const G4double next = holder->GetNextTime();  // DBL_MAX when no list is left
  const G4double now = scheduler->GetGlobalTime();
  if (next < scheduler->GetEndTime() && (!(now < next) || !holder->MainListsNOTEmpty())) TakeSnapshot();
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

  // CheckStage has just appended this stage to Activations().Stages().
  try
  {
    CountContacts(live, activated, fCheck->Stages().size() - 1, now / ns);
  }
  catch (const std::exception& e)
  {
    Fail(std::string("new-old contact count: ") + e.what());
  }
}

void ActivationObserver::CountContacts(const std::vector<const G4Track*>& live,
                                       const std::vector<std::size_t>& activated, std::size_t stageIndex,
                                       double timeNs)
{
  const auto start = std::chrono::steady_clock::now();
  std::vector<bool> isFresh(live.size(), false);
  for (const auto i : activated) isFresh[i] = true;
  std::vector<ContactMolecule> fresh, old;
  fresh.reserve(activated.size());
  old.reserve(live.size() - activated.size());
  for (std::size_t i = 0; i < live.size(); ++i)
  {
    const auto& p = live[i]->GetPosition();
    ContactMolecule molecule;
    molecule.species = fRadii.Label(GetMolecule(live[i])->GetMolecularConfiguration());
    molecule.positionNm = {p.x() / nm, p.y() / nm, p.z() / nm};
    (isFresh[i] ? fresh : old).push_back(std::move(molecule));
  }
  const auto counts = CountNewOldContacts(fresh, old, fRadii.AsFunction());

  fContacts.pairs += counts.pairs;
  fContacts.newMoleculesInContact += counts.newMoleculesInContact;
  for (const auto& [label, n] : counts.perPair) fContacts.perPair[label] += n;
  fContacts.perActivation.push_back(ContactActivation{stageIndex, timeNs, counts.pairs, counts.newMoleculesInContact});
  fContacts.cpuSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

void ActivationObserver::UserReactionAction(const G4Track&, const G4Track&, const std::vector<G4Track*>* products)
{
  const auto* scheduler = G4Scheduler::Instance();
  if (scheduler->GetTimeStep() == 0 && scheduler->GetGlobalTime() == scheduler->GetStartTime())
    ++fStartZeroStepReactions;
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
  try
  {
    Finish();
  }
  catch (const std::exception& e)
  {
    Fail(std::string("end-of-history check: ") + e.what());
  }
}

void ActivationObserver::Finish()
{
  auto* scheduler = G4Scheduler::Instance();
  fEnd.steps = scheduler->GetNbSteps();
  fEnd.finalTime = scheduler->GetGlobalTime();
  const G4double endTime = scheduler->GetEndTime();

  const auto live = LiveTracks();
  const auto activation = fCheck->CheckEnd(CheckTracks(live));
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

  // Species counts on the record grid, summed per species name.
  std::map<std::pair<double, std::string>, double> counts;
  const auto allNames = AllSpeciesNames();
  for (const auto timeNs : fRecordTimesNs)
    for (const auto& name : allNames) counts[{timeNs, name}] = 0;
  for (const auto timeNs : fRecordTimesNs)
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

}  // namespace ptirt
