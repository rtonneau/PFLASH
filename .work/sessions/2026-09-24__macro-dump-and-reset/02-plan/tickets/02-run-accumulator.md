# Ticket 02: run-accumulator

**Acceptance Criteria:**
- [ ] New `RunAccumulator` namespace (`header/RunAccumulator.hh` / `src/RunAccumulator.cc`) holds process-wide persistent state: accumulated energy, an accumulated `ReactionCounter`, an accumulated `PhysicsInteractionCounter`, a pending-data flag, and an in-memory set of prefixes already used this process.
- [ ] `Accumulate(energy, reactions, interactions)` merges into the persistent totals (via `ReactionCounter::Merge`/`PhysicsInteractionCounter::Merge`, energy addition) without modifying its inputs, and sets the pending-data flag.
- [ ] `HasPendingData()` reflects `Accumulate()`/`ClearAccumulated()` calls correctly.
- [ ] `ClearAccumulated()` resets energy to 0, both counters to empty, and the pending-data flag to false.
- [ ] `TryReservePrefix(prefix, enforceUniqueness, err)` returns `true` and records `prefix` as used on first use; with `enforceUniqueness=true`, a repeat of the same `prefix` returns `false` and sets `err` without changing state; with `enforceUniqueness=false`, repeats always succeed.
- [ ] `RunAccumulator.cc` has zero dependency on `G4SDManager`, `ScoreSpecies`, or `G4AnalysisManager` — only `ReactionCounter`, `PhysicsInteractionCounter`, and `globals.hh` — so it never needs a live Geant4 kernel to exercise.
- [ ] `test/RunAccumulatorTest.cc` covers all of the above (see Notes for full test code) and is registered in `CMakeLists.txt`.
- [ ] `ctest --test-dir build-ninja -R RunAccumulatorTest --output-on-failure` passes.

**Files to Touch:**
- Create: `header/RunAccumulator.hh`
- Create: `src/RunAccumulator.cc`
- Create: `test/RunAccumulatorTest.cc`
- Modify: `CMakeLists.txt`

**Verification Step:**

Run:
```bash
cmake --build build-ninja --target RunAccumulatorTest
ctest --test-dir build-ninja -R RunAccumulatorTest --output-on-failure
```

Expected:
```
100% tests passed, 0 tests failed out of 1
```
(and the test binary's own final line, `All RunAccumulator tests passed.`)

**Notes:**

This ticket depends on ticket 01 only in the sense that it should be
implemented after it (per the plan's sequencing), but `RunAccumulator`
itself does not call `OutputDir` at all — that coupling is ticket 03's
job (`RunAccumulatorMessenger`). This keeps `RunAccumulator.cc`'s test
footprint identical in shape to the existing `ReactionCounterTest`
(pure logic, `${Geant4_LIBRARIES}` only for `G4String`/`G4double`).

Step 1 — write `header/RunAccumulator.hh`:

```cpp
/// \file RunAccumulator.hh
/// \brief Process-wide accumulator for data that must survive across
/// /run/beamOn calls until explicitly flushed.
///
/// Species yields don't need this -- the ScoreSpecies scorer is itself a
/// persistent, SD-registered object and accumulates on its own. Energy
/// deposit and the two counters (ReactionCounter, PhysicsInteractionCounter)
/// live on the per-run Run object today, which is destroyed every beamOn --
/// RunAccumulator gives them a persistent home instead, fed once per run
/// from RunAction::EndOfRunAction.
///
/// Pure accumulation/bookkeeping: no file I/O, no Geant4 kernel dependency
/// beyond G4double/G4String. RunAccumulatorMessenger owns the file-writing
/// side (species lookup, OutputDir, G4AnalysisManager) and the public
/// UI-command / exit-time entry points.

#ifndef RunAccumulator_h
#define RunAccumulator_h 1

#include "ReactionCounter.hh"
#include "PhysicsInteractionCounter.hh"

#include "globals.hh"

namespace RunAccumulator
{
  /// Merges energy/reactions/interactions into the persistent totals
  /// (reactions and interactions are merged via ReactionCounter::Merge /
  /// PhysicsInteractionCounter::Merge -- their arguments are left
  /// unmodified) and marks pending data. Called once per run from
  /// RunAction::EndOfRunAction (master thread only).
  void Accumulate(G4double energy, const ReactionCounter &reactions,
                   const PhysicsInteractionCounter &interactions);

  /// True if Accumulate() has added data since the last ClearAccumulated().
  G4bool HasPendingData();

  G4double GetAccumulatedEnergy();
  const ReactionCounter &GetAccumulatedReactionCounter();
  const PhysicsInteractionCounter &GetAccumulatedInteractionCounter();

  /// Resets energy to 0 and both counters to empty, and clears the
  /// pending-data flag. Does not touch the prefix-uniqueness set.
  void ClearAccumulated();

  /// If enforceUniqueness is false, or prefix hasn't been reserved before,
  /// records prefix as used and returns true. If enforceUniqueness is true
  /// and prefix was already reserved earlier in this process, leaves state
  /// unchanged, sets err, and returns false.
  G4bool TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                           G4String &err);
}

#endif // RunAccumulator_h
```

Step 2 — write `src/RunAccumulator.cc`:

```cpp
/// \file RunAccumulator.cc
/// \brief Implementation of RunAccumulator

#include "RunAccumulator.hh"

#include <set>

namespace
{
  G4double gAccumulatedEnergy = 0.;
  ReactionCounter gAccumulatedReactionCounter;
  PhysicsInteractionCounter gAccumulatedInteractionCounter;
  G4bool gHasPendingData = false;
  std::set<G4String> gUsedPrefixes;
}

void RunAccumulator::Accumulate(G4double energy, const ReactionCounter &reactions,
                                 const PhysicsInteractionCounter &interactions)
{
  gAccumulatedEnergy += energy;
  gAccumulatedReactionCounter.Merge(reactions);
  gAccumulatedInteractionCounter.Merge(interactions);
  gHasPendingData = true;
}

G4bool RunAccumulator::HasPendingData()
{
  return gHasPendingData;
}

G4double RunAccumulator::GetAccumulatedEnergy()
{
  return gAccumulatedEnergy;
}

const ReactionCounter &RunAccumulator::GetAccumulatedReactionCounter()
{
  return gAccumulatedReactionCounter;
}

const PhysicsInteractionCounter &RunAccumulator::GetAccumulatedInteractionCounter()
{
  return gAccumulatedInteractionCounter;
}

void RunAccumulator::ClearAccumulated()
{
  gAccumulatedEnergy = 0.;
  gAccumulatedReactionCounter.Clear();
  gAccumulatedInteractionCounter.Clear();
  gHasPendingData = false;
}

G4bool RunAccumulator::TryReservePrefix(const G4String &prefix, G4bool enforceUniqueness,
                                         G4String &err)
{
  if (enforceUniqueness && gUsedPrefixes.count(prefix) > 0)
  {
    err = "prefix '" + prefix + "' was already used earlier in this run -- choose a different prefix";
    return false;
  }

  gUsedPrefixes.insert(prefix);
  return true;
}
```

Step 3 — write `test/RunAccumulatorTest.cc`:

```cpp
/// \file RunAccumulatorTest.cc
/// \brief Plain-assert unit tests for RunAccumulator (no test framework, no
/// Geant4 runtime -- exercises pure accumulation/prefix-bookkeeping logic
/// only. DumpAndReset's file-writing/species lookup lives in
/// RunAccumulatorMessenger and is integration-verified via sim.exe instead.)

#include "RunAccumulator.hh"

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
  RunAccumulator::Accumulate(1 * CLHEP::keV, reactions, interactions);

  assert(RunAccumulator::HasPendingData());

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateSumsEnergyAcrossCalls()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  PhysicsInteractionCounter interactions;
  RunAccumulator::Accumulate(1 * CLHEP::keV, reactions, interactions);
  RunAccumulator::Accumulate(2 * CLHEP::keV, reactions, interactions);

  assert(RunAccumulator::GetAccumulatedEnergy() == 3 * CLHEP::keV);

  RunAccumulator::ClearAccumulated();
}

static void TestAccumulateMergesReactionCounts()
{
  RunAccumulator::ClearAccumulated();

  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;

  RunAccumulator::Accumulate(0., reactions, interactions);
  RunAccumulator::Accumulate(0., reactions, interactions);

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

  RunAccumulator::Accumulate(0., reactions, interactions);
  RunAccumulator::Accumulate(0., reactions, interactions);

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

  RunAccumulator::Accumulate(0., reactions, interactions);

  assert(reactions.GetCounts().at(1 * CLHEP::picosecond).at("H + H -> H2") == 1);
  assert(interactions.GetCounts().at("e-_G4DNAIonisation") == 1);

  RunAccumulator::ClearAccumulated();
}

// --- ClearAccumulated -------------------------------------------------

static void TestClearAccumulatedResetsEverything()
{
  ReactionCounter reactions;
  reactions.Record("H + H -> H2", 1 * CLHEP::picosecond);
  PhysicsInteractionCounter interactions;
  interactions.Record("e-_G4DNAIonisation");
  RunAccumulator::Accumulate(5 * CLHEP::keV, reactions, interactions);

  RunAccumulator::ClearAccumulated();

  assert(!RunAccumulator::HasPendingData());
  assert(RunAccumulator::GetAccumulatedEnergy() == 0.);
  assert(RunAccumulator::GetAccumulatedReactionCounter().GetCounts().empty());
  assert(RunAccumulator::GetAccumulatedInteractionCounter().GetCounts().empty());
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

int main()
{
  TestAccumulateSetsPendingFlag();
  TestAccumulateSumsEnergyAcrossCalls();
  TestAccumulateMergesReactionCounts();
  TestAccumulateMergesInteractionCounts();
  TestAccumulateDoesNotModifyItsInputs();

  TestClearAccumulatedResetsEverything();

  TestTryReservePrefixAcceptsFirstUse();
  TestTryReservePrefixRefusesRepeatWhenEnforced();
  TestTryReservePrefixAllowsRepeatWhenNotEnforced();
  TestTryReservePrefixTreatsEmptyPrefixAsReservable();

  std::cout << "All RunAccumulator tests passed." << std::endl;
  return 0;
}
```

Note: `TestTryReservePrefixTreatsEmptyPrefixAsReservable` uses `""` --
since it runs after the other `TryReservePrefix` tests (which all use
non-empty, distinct prefixes), `""` is guaranteed unused when it starts.

Step 4 — register the new test executable in `CMakeLists.txt`. Insert
this block right after the existing `PhysicsInteractionCounterTest`
block (after the line `add_test(NAME PhysicsInteractionCounterTest
COMMAND PhysicsInteractionCounterTest)`, before the `# Copy macro
files...` comment):

```cmake
# Standalone unit test for RunAccumulator (pure accumulation/prefix
# bookkeeping logic, no Geant4 kernel dependency -- DumpAndReset's
# file-writing/species lookup lives in RunAccumulatorMessenger instead,
# which is not unit-tested, matching this project's existing convention).
add_executable(RunAccumulatorTest test/RunAccumulatorTest.cc src/RunAccumulator.cc src/ReactionCounter.cc src/PhysicsInteractionCounter.cc src/OutputDir.cc)
target_link_libraries(RunAccumulatorTest ${Geant4_LIBRARIES})
target_include_directories(RunAccumulatorTest PRIVATE ${project_include_dirs})
add_test(NAME RunAccumulatorTest COMMAND RunAccumulatorTest)
```

(`src/OutputDir.cc` is linked because `ReactionCounter.cc` calls
`OutputDir::Resolve()` inside `WriteCsv()` -- same reason
`ReactionCounterTest`'s target links it, even though this test never
calls `WriteCsv`.)

Step 5 — build and run:

```bash
cmake --build build-ninja --target RunAccumulatorTest
ctest --test-dir build-ninja -R RunAccumulatorTest --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1`.

Step 6 — commit:

```bash
git add header/RunAccumulator.hh src/RunAccumulator.cc test/RunAccumulatorTest.cc CMakeLists.txt
git commit -m "feat: add RunAccumulator for cross-run energy/reaction/interaction data"
```
