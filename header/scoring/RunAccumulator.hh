/// \file RunAccumulator.hh
/// \brief Process-wide accumulator for data that must survive across
/// /run/beamOn calls until explicitly flushed.
///
/// Species yields don't need this -- the ScoreSpecies scorer is itself a
/// persistent, SD-registered object and accumulates on its own. Energy
/// deposit and the three counters (ReactionCounter, PhysicsInteractionCounter,
/// MesoSpeciesCounter) live on the per-run Run object today, which is destroyed every beamOn --
/// RunAccumulator gives them a persistent home instead, fed once per run
/// from RunAction::EndOfRunAction.
///
/// Pure accumulation/bookkeeping: no file I/O, no Geant4 kernel dependency
/// beyond G4double/G4String. RunAccumulatorMessenger owns the file-writing
/// side (species lookup, OutputDir, G4AnalysisManager) and the public
/// UI-command / exit-time entry points.

#ifndef RunAccumulator_h
#define RunAccumulator_h 1

#include "scoring/DataNode.hh"
#include "scoring/MesoSpeciesCounter.hh"
#include "scoring/ReactionCounter.hh"
#include "scoring/PhysicsInteractionCounter.hh"
#include "scoring/TrackLengthTable.hh"

#include "globals.hh"

#include <vector>

namespace RunAccumulator
{
  /// Merges energy/events/reactions/interactions/mesoscopic species counts
  /// and the per-event track-length rows into the persistent totals (the
  /// counters and the table are merged via their Merge() -- the arguments
  /// are left unmodified) and marks pending data. Called once per run from
  /// RunAction::EndOfRunAction (master thread only).
  void Accumulate(G4double energy, long events, const ReactionCounter &reactions,
                   const PhysicsInteractionCounter &interactions,
                   const MesoSpeciesCounter &mesoSpecies,
                   const TrackLengthTable &trackLengths);

  /// True if Accumulate() has added data since the last ClearAccumulated().
  G4bool HasPendingData();

  G4double GetAccumulatedEnergy();
  long GetAccumulatedEvents();
  const ReactionCounter &GetAccumulatedReactionCounter();
  const PhysicsInteractionCounter &GetAccumulatedInteractionCounter();
  const MesoSpeciesCounter &GetAccumulatedMesoSpeciesCounter();
  const TrackLengthTable &GetAccumulatedTrackLengthTable();

  /// Appends one run's manifest entry (built by RunManifest::RecordRun) for
  /// the manifest of the next dump. Called once per run (master thread only),
  /// next to Accumulate(); does not set the pending-data flag.
  void AddRunEntry(const DataNode &entry);

  /// The entries added since the last ClearAccumulated(), in call order.
  const std::vector<DataNode> &GetRunEntries();

  /// Resets energy and events to 0, the three counters and the track-length
  /// table to empty and the run entries to none, and clears the pending-data flag. Does not touch the
  /// prefix-uniqueness set.
  void ClearAccumulated();

  /// If enforceUniqueness is false, or prefix hasn't been reserved before,
  /// records prefix as used and returns true. If enforceUniqueness is true
  /// and prefix was already reserved earlier in this process, leaves state
  /// unchanged, sets err, and returns false.
  G4bool TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                           G4String &err);

  /// Same contract as TryReservePrefix, for dump subfolder names. Kept in
  /// a separate set: a prefix and a subfolder never collide with each other.
  G4bool TryReserveSubdir(const G4String &subdir, G4bool enforceUniqueness,
                           G4String &err);
}

#endif // RunAccumulator_h
