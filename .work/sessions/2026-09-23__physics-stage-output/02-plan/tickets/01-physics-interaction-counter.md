# Ticket 01: physics-interaction-counter

**Acceptance Criteria:**
- [ ] `header/PhysicsInteractionCounter.hh` and `src/PhysicsInteractionCounter.cc` exist, implementing `Record(const G4String&)`, `Merge(const PhysicsInteractionCounter&)`, `Clear()`, `GetCounts() const -> const std::map<G4String, G4long>&`, `WriteAscii(std::ostream&) const`, `WriteCsv(std::ostream&) const`.
- [ ] The class has zero dnachem-min-specific includes (no `OutputDir.hh`, `DnaLogger.hh`, or any other project header) — only `globals.hh` and the standard library — documented in a top-of-file comment mirroring `src/PureWaterReactions.cc`'s portability convention.
- [ ] `test/PhysicsInteractionCounterTest.cc` exists and covers: single-record count, repeated-record accumulation, distinct labels staying separate, merge summing overlapping entries, merge not modifying the source, merge adding entries only present in the source, clear emptying counts, `WriteAscii` line format, `WriteCsv` header + row format.
- [ ] `CMakeLists.txt` has a new `PhysicsInteractionCounterTest` target (linking only `src/PhysicsInteractionCounter.cc`, no other project `.cc`) registered via `add_test`.
- [ ] `ctest --test-dir build-ninja -R PhysicsInteractionCounterTest --output-on-failure` passes.

**Files to Touch:**
- `header/PhysicsInteractionCounter.hh`
- `src/PhysicsInteractionCounter.cc`
- `test/PhysicsInteractionCounterTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```powershell
cmake --build build-ninja --config Debug --target PhysicsInteractionCounterTest
ctest --test-dir build-ninja -R PhysicsInteractionCounterTest --output-on-failure
```

Expected:
`100% tests passed, 0 tests failed out of 1` for `PhysicsInteractionCounterTest`.

**Notes:**

Mirror `header/ReactionCounter.hh` / `src/ReactionCounter.cc` / `test/ReactionCounterTest.cc` for style and the MSVC Debug-CRT-dialog guard, but this class is simpler: no time binning, no bin-edge parsing, no id-map, no `G4AnalysisManager`/ntuple output, no `OutputDir` coupling.

`header/PhysicsInteractionCounter.hh`:

```cpp
/// \file PhysicsInteractionCounter.hh
/// \brief Portable string-frequency counter for physical interaction counts
///
/// Self-contained, project-agnostic unit: no OutputDir, DnaLogger, or other
/// dnachem-min-specific includes -- only globals.hh (G4String) and the
/// standard library. Copy-paste portable to another Geant4-DNA project.
///
/// Counts occurrences of whatever label callers pass to Record() -- it has
/// no notion of "process" or "G4DNA"; callers decide what a label means and
/// whether to record it at all.

#ifndef PhysicsInteractionCounter_h
#define PhysicsInteractionCounter_h 1

#include "globals.hh"

#include <iosfwd>
#include <map>

class PhysicsInteractionCounter
{
 public:
  void Record(const G4String& label);

  /// Adds `other`'s counts into this counter; `other` is left unchanged.
  void Merge(const PhysicsInteractionCounter& other);

  void Clear();

  using Counts = std::map<G4String, G4long>;
  const Counts& GetCounts() const { return fCounts; }

  /// Writes one "<label>    count = N" line per label, sorted by label.
  void WriteAscii(std::ostream& out) const;

  /// Writes a "label,count" header followed by one row per label.
  void WriteCsv(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // PhysicsInteractionCounter_h
```

`src/PhysicsInteractionCounter.cc`:

```cpp
/// \file PhysicsInteractionCounter.cc
/// \brief Implementation of the PhysicsInteractionCounter class

#include "PhysicsInteractionCounter.hh"

#include <ostream>

void PhysicsInteractionCounter::Record(const G4String& label)
{
  ++fCounts[label];
}

void PhysicsInteractionCounter::Merge(const PhysicsInteractionCounter& other)
{
  for (const auto& [label, count] : other.fCounts) {
    fCounts[label] += count;
  }
}

void PhysicsInteractionCounter::Clear()
{
  fCounts.clear();
}

void PhysicsInteractionCounter::WriteAscii(std::ostream& out) const
{
  for (const auto& [label, count] : fCounts) {
    out << label << "    count = " << count << "\n";
  }
}

void PhysicsInteractionCounter::WriteCsv(std::ostream& out) const
{
  out << "label,count\n";
  for (const auto& [label, count] : fCounts) {
    out << label << "," << count << "\n";
  }
}
```

`test/PhysicsInteractionCounterTest.cc`:

```cpp
/// \file PhysicsInteractionCounterTest.cc
/// \brief Plain-assert unit tests for PhysicsInteractionCounter (no test
/// framework, no Geant4 runtime -- exercises pure accumulation/merge logic
/// only).

#include "PhysicsInteractionCounter.hh"

#include <cassert>
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static void TestRecordSingleLabelIncrementsCount()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");

  assert(counter.GetCounts().at("e-_G4DNAIonisation") == 1);
}

static void TestRecordSameLabelTwiceAccumulates()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");
  counter.Record("e-_G4DNAIonisation");

  assert(counter.GetCounts().at("e-_G4DNAIonisation") == 2);
}

static void TestRecordDifferentLabelsStaySeparate()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");
  counter.Record("e-_G4DNAExcitation");

  assert(counter.GetCounts().at("e-_G4DNAIonisation") == 1);
  assert(counter.GetCounts().at("e-_G4DNAExcitation") == 1);
}

static void TestMergeSumsOverlappingEntries()
{
  PhysicsInteractionCounter a;
  a.Record("e-_G4DNAIonisation");
  PhysicsInteractionCounter b;
  b.Record("e-_G4DNAIonisation");
  b.Record("e-_G4DNAIonisation");

  a.Merge(b);

  assert(a.GetCounts().at("e-_G4DNAIonisation") == 3);
}

static void TestMergeAddsEntriesOnlyPresentInOther()
{
  PhysicsInteractionCounter a;
  PhysicsInteractionCounter b;
  b.Record("e-_G4DNAAttachment");

  a.Merge(b);

  assert(a.GetCounts().at("e-_G4DNAAttachment") == 1);
}

static void TestMergeDoesNotModifyOther()
{
  PhysicsInteractionCounter a;
  PhysicsInteractionCounter b;
  b.Record("e-_G4DNAIonisation");

  a.Merge(b);

  assert(b.GetCounts().at("e-_G4DNAIonisation") == 1);
}

static void TestClearEmptiesCounts()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");

  counter.Clear();

  assert(counter.GetCounts().empty());
}

static void TestWriteAsciiFormatsLabelCountLines()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");
  counter.Record("e-_G4DNAIonisation");

  std::ostringstream out;
  counter.WriteAscii(out);

  G4String text = out.str();
  assert(text.find("e-_G4DNAIonisation    count = 2") != G4String::npos);
}

static void TestWriteCsvFormatsHeaderAndRows()
{
  PhysicsInteractionCounter counter;
  counter.Record("e-_G4DNAIonisation");

  std::ostringstream out;
  counter.WriteCsv(out);

  G4String text = out.str();
  assert(text.find("label,count") != G4String::npos);
  assert(text.find("e-_G4DNAIonisation,1") != G4String::npos);
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestRecordSingleLabelIncrementsCount();
  TestRecordSameLabelTwiceAccumulates();
  TestRecordDifferentLabelsStaySeparate();

  TestMergeSumsOverlappingEntries();
  TestMergeAddsEntriesOnlyPresentInOther();
  TestMergeDoesNotModifyOther();

  TestClearEmptiesCounts();

  TestWriteAsciiFormatsLabelCountLines();
  TestWriteCsvFormatsHeaderAndRows();

  std::cout << "All PhysicsInteractionCounter tests passed." << std::endl;
  return 0;
}
```

In `CMakeLists.txt`, immediately after the `ReactionCounterTest` block (after `add_test(NAME ReactionCounterTest COMMAND ReactionCounterTest)`), insert:

```cmake
# Standalone unit test for PhysicsInteractionCounter (fully generic,
# portable, no dnachem-min-specific dependency -- no OutputDir.cc needed).
add_executable(PhysicsInteractionCounterTest test/PhysicsInteractionCounterTest.cc src/PhysicsInteractionCounter.cc)
target_link_libraries(PhysicsInteractionCounterTest ${Geant4_LIBRARIES})
target_include_directories(PhysicsInteractionCounterTest PRIVATE ${project_include_dirs})
add_test(NAME PhysicsInteractionCounterTest COMMAND PhysicsInteractionCounterTest)
```
