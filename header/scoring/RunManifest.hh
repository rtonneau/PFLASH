/// \file RunManifest.hh
/// \brief Geant4-facing collector that writes a dump's Manifest.json.
///
/// The one place that lists the manifest's entries: RecordRun builds each
/// run's entry, Write builds the top-level tree from the live process
/// (selected Chemistry, environment, run manager, output directory,
/// RunAccumulator's totals and run entries). Both use the portable,
/// format-neutral DataNode tree, serialised by the generic JsonWriter --
/// adding or removing an entry is one Add() line here. Write is called once
/// per dump from RunAccumulatorMessenger::WriteAllAndReset, after the data
/// files are written and before the accumulators are cleared.

#ifndef RunManifest_h
#define RunManifest_h 1

#include "globals.hh"

#include <string>
#include <vector>

class Run;

namespace RunManifest
{
  /// Records the macro file the process was started with, for the manifest
  /// (called once from sim.cc; empty if never set, e.g. GUI mode).
  void SetMacroName(const G4String &macro);

  /// Marks the process start (steady clock), the origin of the manifest's
  /// elapsedSinceStart_s. Called once at the top of main() in sim.cc.
  void MarkProcessStart();

  /// Builds this run's manifest entry (run id, events, beam or nulls,
  /// energy deposit, seed, wall time) and stores it with
  /// RunAccumulator::AddRunEntry. `wallTime_s` is the run's wall-clock
  /// duration (BeginOfRunAction to EndOfRunAction). Called once per run from
  /// RunAction::EndOfRunAction (master thread only).
  void RecordRun(const Run &run, double wallTime_s);

  /// Writes Manifest.json (through OutputDir::Resolve, so the dump's prefix
  /// or subfolder applies). `files` are the data files this dump wrote, as
  /// names relative to the manifest's folder. A file that cannot be opened
  /// raises a JustWarning G4Exception; the data files are already on disk.
  ///
  /// Also rewrites the results index, <outdir>/Manifest.json (ResultsIndex),
  /// listing every dump so far, the exit-time EndOfRun_ flush included. A
  /// dump with an empty prefix and no subfolder has its manifest at that very
  /// path: the file is then written once, as that manifest plus the
  /// top-level "dumps" array (also rewritten by every later dump), so neither
  /// overwrites the other. A failed open of the index is a JustWarning.
  void Write(const G4String &prefix, const G4String &subdir,
             const std::vector<std::string> &files);
} // namespace RunManifest

#endif // RunManifest_h
