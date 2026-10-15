/// \file RunAccumulatorTest.cc
/// \brief Plain-assert unit tests for RunAccumulator (no test framework, no
/// Geant4 runtime -- exercises pure accumulation/prefix-bookkeeping logic
/// only. DumpAndReset's file-writing/species lookup lives in
/// RunAccumulatorMessenger and is integration-verified via sim.exe instead.)

#include "scoring/RunAccumulator.hh"

#include "G4SystemOfUnits.hh"

#include <cassert>
#include <iostream>

// --- Accumulate / HasPendingData / GetAccumulated* -------------------------

static void TestAccumulateSetsPendingFlag()
{
  RunAccumulator::ClearAccumulated();
  assert(!RunAccumulator::HasPendingData());

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;
  RunAccumulator::Accumulate(1 * CLHEP::keV, 0, reactions, interactions, meso, trackLengths);

  assert(RunAccumulator::HasPendingData());

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateSumsEnergyAcrossCalls()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;
  RunAccumulator::Accumulate(1 * CLHEP::keV, 0, reactions, interactions, meso, trackLengths);
  RunAccumulator::Accumulate(2 * CLHEP::keV, 0, reactions, interactions, meso, trackLengths);

  assert(RunAccumulator::GetAccumulatedEnergy() == 3 * CLHEP::keV);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateSumsEventsAcrossCalls()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;
  RunAccumulator::Accumulate(0., 2, reactions, interactions, meso, trackLengths);
  RunAccumulator::Accumulate(0., 3, reactions, interactions, meso, trackLengths);

  assert(RunAccumulator::GetAccumulatedEvents() == 5);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesReactionCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;

  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);
  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);

  assert(RunAccumulator::GetAccumulatedReactionCounter()
             .GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 2);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesInteractionCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;

  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);
  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);

  assert(RunAccumulator::GetAccumulatedInteractionCounter()
             .GetCounts().at("e-_G4DNAIonisation") == 2);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateDoesNotModifyItsInputs()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;

  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);

  assert(reactions.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 1);
  assert(interactions.GetCounts().at("e-_G4DNAIonisation") == 1);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesMesoSpeciesCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;
  meso.Add(5., "OH^0", 3);

  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);
  RunAccumulator::Accumulate(0., 0, reactions, interactions, meso, trackLengths);

  assert(!RunAccumulator::GetAccumulatedMesoSpeciesCounter().Empty());
  assert(RunAccumulator::GetAccumulatedMesoSpeciesCounter().GetCounts().at(5.).at("OH^0") == 6);
  assert(meso.GetCounts().at(5.).at("OH^0") == 3);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesTrackLengthRowsAcrossRuns()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  MesoSpeciesCounter meso;

  TrackLengthTable run0;
  TrackLengthTable::Row row;
  row.run = 0;
  row.event = 0;
  row.primaryLength_nm = 2500.;
  run0.Add(row);
  row.event = 1;
  run0.Add(row);

  TrackLengthTable run1;
  row.run = 1;
  row.event = 0;
  run1.Add(row);

  RunAccumulator::Accumulate(0., 2, reactions, interactions, meso, run0);
  RunAccumulator::Accumulate(0., 1, reactions, interactions, meso, run1);

  const TrackLengthTable &acc = RunAccumulator::GetAccumulatedTrackLengthTable();
  assert(acc.Size() == 3);
  assert(acc.GetRows()[2].run == 1);
  assert(acc.GetRows()[0].primaryLength_nm == 2500.);
  // Inputs are left unmodified.
  assert(run0.Size() == 2);
  assert(run1.Size() == 1);

  RunAccumulator::ClearAccumulated();
}

// --- ClearAccumulated -------------------------------------------------

static void TestClearAccumulatedResetsEverything()
{
  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");
  MesoSpeciesCounter meso;
  TrackLengthTable trackLengths;
  meso.Add(5., "OH^0", 3);
  TrackLengthTable::Row row;
  row.event = 1;
  trackLengths.Add(row);
  RunAccumulator::Accumulate(5 * CLHEP::keV, 4, reactions, interactions, meso, trackLengths);

  RunAccumulator::ClearAccumulated();

  assert(!RunAccumulator::HasPendingData());
  assert(RunAccumulator::GetAccumulatedEnergy() == 0.);
  assert(RunAccumulator::GetAccumulatedEvents() == 0);
  assert(RunAccumulator::GetAccumulatedReactionCounter().GetCounts().empty());
  assert(RunAccumulator::GetAccumulatedInteractionCounter().GetCounts().empty());
  assert(RunAccumulator::GetAccumulatedMesoSpeciesCounter().Empty());
  assert(RunAccumulator::GetAccumulatedTrackLengthTable().Empty());
}

// --- TryReservePrefix --------------------------------------------------

static void TestTryReservePrefixAcceptsFirstUse()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_first_", true, err));
  assert(err.empty());
}

static void TestTryReservePrefixRefusesRepeatWhenEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_repeat_", true, err));

  G4String err2;
  assert(!RunAccumulator::TryReservePrefix("ticket02_repeat_", true, err2));
  assert(!err2.empty());
}

static void TestTryReservePrefixAllowsRepeatWhenNotEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("ticket02_unenforced_", true, err));

  G4String err2;
  assert(RunAccumulator::TryReservePrefix("ticket02_unenforced_", false, err2));
  assert(err2.empty());
}

static void TestTryReservePrefixTreatsEmptyPrefixAsReservable()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("", true, err));

  G4String err2;
  assert(!RunAccumulator::TryReservePrefix("", true, err2));
}

// --- TryReserveSubdir --------------------------------------------------

static void TestTryReserveSubdirAcceptsFirstUse()
{
  G4String err;
  assert(RunAccumulator::TryReserveSubdir("subdir_first", true, err));
  assert(err.empty());
}

static void TestTryReserveSubdirRefusesRepeatWhenEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReserveSubdir("subdir_repeat", true, err));

  G4String err2;
  assert(!RunAccumulator::TryReserveSubdir("subdir_repeat", true, err2));
  assert(!err2.empty());
}

static void TestTryReserveSubdirAllowsRepeatWhenNotEnforced()
{
  G4String err;
  assert(RunAccumulator::TryReserveSubdir("subdir_unenforced", true, err));

  G4String err2;
  assert(RunAccumulator::TryReserveSubdir("subdir_unenforced", false, err2));
  assert(err2.empty());
}

static void TestSubdirAndPrefixReservationsAreIndependent()
{
  G4String err;
  assert(RunAccumulator::TryReservePrefix("shared_name", true, err));
  assert(RunAccumulator::TryReserveSubdir("shared_name", true, err));
}

// --- AddRunEntry / GetRunEntries -------------------------------------------

static void TestRunEntriesAccumulateAndClear()
{
  RunAccumulator::ClearAccumulated();

  RunAccumulator::AddRunEntry(DataNode::MakeObject().Add("run", 3));
  RunAccumulator::AddRunEntry(DataNode::MakeObject().Add("run", 4));

  const std::vector<DataNode> &entries = RunAccumulator::GetRunEntries();
  assert(entries.size() == 2);
  assert(entries[0].GetMembers()[0].second.GetInteger() == 3);
  assert(entries[1].GetMembers()[0].second.GetInteger() == 4);

  RunAccumulator::ClearAccumulated();
  assert(RunAccumulator::GetRunEntries().empty());
}

static void TestAddRunEntryDoesNotSetPendingFlag()
{
  RunAccumulator::ClearAccumulated();

  RunAccumulator::AddRunEntry(DataNode::MakeObject());
  assert(!RunAccumulator::HasPendingData());

  RunAccumulator::ClearAccumulated();
}

int main()
{
  TestAccumulateSetsPendingFlag();
  TestAccumulateSumsEnergyAcrossCalls();
  TestAccumulateSumsEventsAcrossCalls();
  TestAccumulateMergesReactionCounts();
  TestAccumulateMergesInteractionCounts();
  TestAccumulateDoesNotModifyItsInputs();
  TestAccumulateMergesMesoSpeciesCounts();
  TestAccumulateMergesTrackLengthRowsAcrossRuns();

  TestClearAccumulatedResetsEverything();

  TestRunEntriesAccumulateAndClear();
  TestAddRunEntryDoesNotSetPendingFlag();

  TestTryReservePrefixAcceptsFirstUse();
  TestTryReservePrefixRefusesRepeatWhenEnforced();
  TestTryReservePrefixAllowsRepeatWhenNotEnforced();
  TestTryReservePrefixTreatsEmptyPrefixAsReservable();

  TestTryReserveSubdirAcceptsFirstUse();
  TestTryReserveSubdirRefusesRepeatWhenEnforced();
  TestTryReserveSubdirAllowsRepeatWhenNotEnforced();
  TestSubdirAndPrefixReservationsAreIndependent();

  std::cout << "All RunAccumulator tests passed." << std::endl;
  return 0;
}
