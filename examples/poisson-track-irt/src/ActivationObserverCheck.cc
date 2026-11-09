#include "ActivationObserverCheck.hh"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace ptirt
{

namespace
{

std::string Ns(double timeNs)
{
  std::ostringstream text;
  text.precision(17);
  text << timeNs << " ns";
  return text.str();
}

}  // namespace

ActivationObserverCheck::ActivationObserverCheck(std::vector<Source> sources) : fSources(std::move(sources))
{
  long long nextId = 1;
  for (std::size_t k = 0; k < fSources.size(); ++k)
  {
    const auto& source = fSources[k];
    if (source.firstTrackId != nextId || source.lastTrackId < source.firstTrackId)
      throw std::invalid_argument(Name(k) + ": track IDs " + std::to_string(source.firstTrackId) + ".." +
                                  std::to_string(source.lastTrackId) + " do not continue the contiguous IDs from 1");
    if (!std::isfinite(source.activationTimeNs) || !(source.activationTimeNs >= 0))
      throw std::invalid_argument(Name(k) + ": the activation time must be finite and not negative");
    for (long long id = source.firstTrackId; id <= source.lastTrackId; ++id) fSourceOfMolecule.push_back(k);
    nextId = source.lastTrackId + 1;
    fPending[source.activationTimeNs].push_back(k);
  }
  fActivated.assign(fSourceOfMolecule.size(), 0);
  fObservedNs.assign(fSources.size(), std::numeric_limits<double>::quiet_NaN());
}

std::size_t ActivationObserverCheck::SourceOf(long long id) const
{
  if (id < 1 || id > static_cast<long long>(fSourceOfMolecule.size())) return kNotASource;
  return fSourceOfMolecule[static_cast<std::size_t>(id - 1)];
}

std::string ActivationObserverCheck::Name(std::size_t source) const
{
  return "source " + std::to_string(fSources[source].label);
}

std::size_t ActivationObserverCheck::PendingSourceCount() const
{
  std::size_t count = 0;
  for (const auto& [time, due] : fPending) count += due.size();
  return count;
}

double ActivationObserverCheck::NextActivationTimeNs() const
{
  return fPending.empty() ? std::numeric_limits<double>::infinity() : fPending.begin()->first;
}

std::string ActivationObserverCheck::Snapshot(const std::vector<Track>& live)
{
  fSnapshot.clear();
  fHasSnapshot = false;
  for (const auto& track : live)
    if (!fSnapshot.emplace(track.id, Seen{track.configuration, track.position}).second)
      return "track " + std::to_string(track.id) + " is listed twice in the history";
  fHasSnapshot = true;
  return {};
}

std::string ActivationObserverCheck::CheckStage(double timeNs, const std::vector<Track>& live,
                                                std::vector<std::size_t>& activated)
{
  const std::string at = " at " + Ns(timeNs);
  if (!fHasSnapshot) return "a delayed list joined the history" + at + " without a snapshot of the history before it";
  if (fPending.empty()) return "a delayed list joined the history" + at + ", but every source had already activated";
  const auto due = fPending.begin();
  if (timeNs != due->first)
    return "a delayed list joined the history" + at + ", but the next source is due at " + Ns(due->first);

  std::unordered_set<long long> present;
  std::vector<std::size_t> fresh;               // positions in `live`
  std::map<std::size_t, std::size_t> perSource;  // due source -> molecules seen
  std::size_t preserved = 0;
  for (std::size_t i = 0; i < live.size(); ++i)
  {
    const auto& track = live[i];
    const auto id = std::to_string(track.id);
    const auto source = SourceOf(track.id);
    if (!present.insert(track.id).second)
      return (source != kNotASource ? "duplicate activation: molecule " + id + " of " + Name(source)
                                    : "track " + id) +
             " is twice in the history" + at;

    const auto before = fSnapshot.find(track.id);
    if (before != fSnapshot.end())
    {
      if (before->second.configuration != track.configuration || before->second.position != track.position)
        return "preservation: the injection" + at + " changed track " + id + " (configuration or position)";
      ++preserved;
      continue;
    }

    if (source == kNotASource)
      return "unexpected track " + id + at + ": neither in the history before the injection nor a source molecule";
    if (fActivated[static_cast<std::size_t>(track.id - 1)])
      return "duplicate activation: molecule " + id + " of " + Name(source) + " activated again" + at;
    const double expected = fSources[source].activationTimeNs;
    if (expected != timeNs)
      return std::string(expected > timeNs ? "premature" : "late") + " activation: molecule " + id + " of " +
             Name(source) + " joined" + at + ", due at " + Ns(expected);
    if (track.globalTimeNs != timeNs)
      return "molecule " + id + " of " + Name(source) + " joined" + at + " with global time " +
             Ns(track.globalTimeNs);
    fresh.push_back(i);
    ++perSource[source];
  }

  if (preserved != fSnapshot.size())
    return "preservation: the injection" + at + " removed " + std::to_string(fSnapshot.size() - preserved) +
           " of the " + std::to_string(fSnapshot.size()) + " molecules already in the history";
  for (const auto source : due->second)
  {
    const auto expected = static_cast<std::size_t>(fSources[source].lastTrackId - fSources[source].firstTrackId + 1);
    const auto seen = perSource.count(source) ? perSource.at(source) : 0;
    if (seen != expected)
      return "missing activation: " + Name(source) + " joined" + at + " with " + std::to_string(seen) + " of its " +
             std::to_string(expected) + " molecules";
  }

  for (const auto i : fresh)
  {
    fActivated[static_cast<std::size_t>(live[i].id - 1)] = 1;
    activated.push_back(i);
  }
  for (const auto source : due->second) fObservedNs[source] = timeNs;
  fStages.push_back(Stage{timeNs, preserved, fresh.size(), due->second});
  fPending.erase(due);
  fSnapshot.clear();
  fHasSnapshot = false;
  return {};
}

std::string ActivationObserverCheck::CheckEnd(const std::vector<Track>& live) const
{
  if (!fPending.empty())
  {
    const auto& [time, due] = *fPending.begin();
    return "missing activation: " + Name(due.front()) + ", due at " + Ns(time) + ", never activated (" +
           std::to_string(PendingSourceCount()) + " sources pending at the end)";
  }
  for (const auto& track : live)
  {
    if (track.id < 1) continue;
    const auto source = SourceOf(track.id);
    if (source == kNotASource)
      return "unexpected track " + std::to_string(track.id) + " at the end: positive ID but not a source molecule";
    if (!fActivated[static_cast<std::size_t>(track.id - 1)])
      return "molecule " + std::to_string(track.id) + " of " + Name(source) +
             " is in the history at the end but was never seen activating";
  }
  return {};
}

}  // namespace ptirt
