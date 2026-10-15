/// \file PhysicsInteractionCounterTest.cc
/// \brief Plain-assert unit tests for PhysicsInteractionCounter (no test
/// framework, no Geant4 runtime -- exercises pure accumulation/merge logic
/// only).

#include "scoring/PhysicsInteractionCounter.hh"

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
  // Route Debug-CRT assert failures to stderr instead of a blocking dialog,
  // so a failing assert() exits the process instead of hanging CI/ctest.
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
