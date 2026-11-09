// ActivationObserverCheck: the bookkeeping behind ActivationObserver, without
// Geant4 (pure standard library, covered by test/ActivationObserverCheckTest.cc).
//
// The shared history receives each source (one precursor packet, see
// SourceInjector.hh) through a G4ITTrackHolder delayed list that joins the
// running history at the source's activation time. ActivationObserver shows
// this class the live tracks at two moments:
//   Snapshot    just before a delayed list joins (last step of a stage);
//   CheckStage  just after it joined (G4UserTimeStepAction::NewStage).
// The tracks that are new between the two must be exactly the molecules of the
// sources due at that time: each source molecule activates once (else
// "duplicate activation"), at its own time (else "premature activation" or
// "late activation"), every molecule of a due source is there (else "missing
// activation"), and nothing else appears. Every track of the snapshot must
// still be there with the same configuration and position (the preservation
// check: an injection never removes or moves a molecule already in the
// history).
//
// Source molecules have the positive track IDs given by SourceInjector,
// contiguous from 1. Molecules made during the history (dissociation and
// reaction products) get IDs <= 0 from G4ITTrackHolder and are never sources.
//
// Times and positions are compared exactly: G4Scheduler sets its clock to the
// delayed list's key, which is the global time the molecules were pushed with,
// and nothing moves a molecule between the last step of a stage and the merge.
// Times are in ns (Geant4's internal time unit); positions in any unit.
#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ptirt
{

class ActivationObserverCheck
{
 public:
  struct Source
  {
    std::size_t label = 0;      // used in the messages (the schedule's source index)
    double activationTimeNs = 0;
    long long firstTrackId = 0;  // firstTrackId..lastTrackId
    long long lastTrackId = 0;
  };

  struct Track
  {
    long long id = 0;
    double globalTimeNs = 0;
    const void* configuration = nullptr;  // compared by identity only
    std::array<double, 3> position{};
  };

  // One delayed list that joined the history.
  struct Stage
  {
    double timeNs = 0;
    std::size_t preserved = 0;         // tracks of the snapshot, all still there unchanged
    std::size_t injected = 0;          // source molecules activated at this stage
    std::vector<std::size_t> sources;  // positions in the source list
  };

  // Sources in track-ID order: IDs contiguous from 1, at least one molecule
  // each, finite activation times >= 0. Throws std::invalid_argument otherwise.
  explicit ActivationObserverCheck(std::vector<Source> sources);

  // Records the live tracks just before the next delayed list joins. Returns
  // "" or the failure (a track ID listed twice).
  std::string Snapshot(const std::vector<Track>& live);

  // A delayed list joined at timeNs; `live` holds every live track now. On
  // success returns "" and appends to `activated` the positions in `live` of
  // the source molecules that activated now (the caller counts them).
  // Otherwise returns the failure and records nothing.
  std::string CheckStage(double timeNs, const std::vector<Track>& live, std::vector<std::size_t>& activated);

  // End of the history: every source activated, and every live track with a
  // positive ID is a source molecule that was seen activating. Returns "" or
  // the failure.
  std::string CheckEnd(const std::vector<Track>& live) const;

  const std::vector<Source>& Sources() const { return fSources; }
  const std::vector<Stage>& Stages() const { return fStages; }
  // NaN until the source activated.
  double ObservedActivationTimeNs(std::size_t source) const { return fObservedNs.at(source); }
  std::size_t PendingSourceCount() const;
  // Earliest activation time still pending; +infinity when none is.
  double NextActivationTimeNs() const;

 private:
  static constexpr std::size_t kNotASource = static_cast<std::size_t>(-1);

  struct Seen
  {
    const void* configuration = nullptr;
    std::array<double, 3> position{};
  };

  std::size_t SourceOf(long long id) const;
  std::string Name(std::size_t source) const;

  std::vector<Source> fSources;
  std::vector<std::size_t> fSourceOfMolecule;  // index = track ID - 1
  std::vector<char> fActivated;                // index = track ID - 1
  std::map<double, std::vector<std::size_t>> fPending;  // activation time -> due sources
  std::vector<double> fObservedNs;
  std::unordered_map<long long, Seen> fSnapshot;
  bool fHasSnapshot = false;
  std::vector<Stage> fStages;
};

}  // namespace ptirt
