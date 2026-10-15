/// \file ResultsIndexTest.cc
/// \brief Plain-assert unit tests for ResultsIndex (DataNode + JsonWriter,
/// no Geant4 runtime).

#include "scoring/DataNode.hh"
#include "scoring/JsonWriter.hh"
#include "scoring/ResultsIndex.hh"

#include <cassert>
#include <iostream>
#include <sstream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
  const DataNode *Find(const DataNode &object, const std::string &key)
  {
    for (const auto &member : object.GetMembers())
      if (member.first == key)
        return &member.second;
    return nullptr;
  }

  DataNode RunEntry(int id, int events, int seed)
  {
    DataNode run = DataNode::MakeObject();
    run.Add("run", id);
    run.Add("events", events);
    run.Add("particle", "e-");
    run.Add("beamEnergy_keV", 10.);
    run.Add("energyDeposit_eV", 123.); // not repeated in the index
    run.Add("seed", seed);
    run.Add("wallTime_s", 1.5);        // not repeated in the index
    return run;
  }
}

static void TestDumpEntryForSubfolder()
{
  const DataNode entry = ResultsIndex::MakeDumpEntry("", "sub_01", "2026-10-08T10:00:00", 4,
                                                     {RunEntry(0, 2, 11), RunEntry(1, 2, 12)});
  assert(Find(entry, "folder")->GetString() == "sub_01");
  assert(Find(entry, "prefix")->GetString().empty());
  assert(Find(entry, "manifest")->GetString() == "sub_01/Manifest.json");
  assert(Find(entry, "events")->GetInteger() == 4);

  const DataNode *runs = Find(entry, "runs");
  assert(runs->GetElements().size() == 2);
  const DataNode &second = runs->GetElements()[1];
  assert(Find(second, "run")->GetInteger() == 1);
  assert(Find(second, "seed")->GetInteger() == 12);
  assert(Find(second, "particle")->GetString() == "e-");
  assert(Find(second, "beamEnergy_keV") != nullptr);
  assert(Find(second, "energyDeposit_eV") == nullptr);
  assert(Find(second, "wallTime_s") == nullptr);
}

static void TestDumpEntryForPrefix()
{
  const DataNode entry = ResultsIndex::MakeDumpEntry("EndOfRun_", "", "t", 2, {RunEntry(0, 2, 5)});
  assert(Find(entry, "manifest")->GetString() == "EndOfRun_Manifest.json");
  assert(Find(entry, "prefix")->GetString() == "EndOfRun_");
}

static void TestDumpEntrySkipsMissingBeamAndNonObjectRuns()
{
  DataNode noBeam = DataNode::MakeObject();
  noBeam.Add("run", 3);
  noBeam.Add("events", 1);
  const DataNode entry =
      ResultsIndex::MakeDumpEntry("p_", "", "t", 1, {noBeam, DataNode(5)});
  const DataNode *runs = Find(entry, "runs");
  assert(runs->GetElements().size() == 1);
  assert(Find(runs->GetElements()[0], "particle") == nullptr);
  assert(Find(runs->GetElements()[0], "run")->GetInteger() == 3);
}

static void TestStandaloneIndexListsEveryDumpInOrder()
{
  const std::vector<DataNode> dumps = {
      ResultsIndex::MakeDumpEntry("", "sub_01", "t1", 2, {RunEntry(0, 2, 1)}),
      ResultsIndex::MakeDumpEntry("", "sub_02", "t2", 3, {RunEntry(1, 3, 2)}),
      ResultsIndex::MakeDumpEntry("EndOfRun_", "", "t3", 1, {RunEntry(2, 1, 3)})};
  const DataNode index = ResultsIndex::Build(dumps, nullptr, "/abs/results");

  assert(Find(index, "kind")->GetString() == "resultsIndex");
  assert(Find(index, "schemaVersion")->GetInteger() == 1);
  assert(Find(index, "outputDirAbsolute")->GetString() == "/abs/results");
  const DataNode *list = Find(index, "dumps");
  assert(list->GetElements().size() == 3);
  assert(Find(list->GetElements()[0], "folder")->GetString() == "sub_01");
  assert(Find(list->GetElements()[2], "prefix")->GetString() == "EndOfRun_");

  // Serialises through the generic writer.
  std::ostringstream out;
  JsonWriter::Write(out, index);
  assert(out.str().find("\"dumps\"") != std::string::npos);
  assert(out.str().find("sub_02/Manifest.json") != std::string::npos);
  assert(out.str().find("EndOfRun_Manifest.json") != std::string::npos);
}

static void TestIndexOnTopOfFlatManifestKeepsBothValid()
{
  // Manifest of an empty-prefix flat dump: the index must not overwrite it.
  DataNode base = DataNode::MakeObject();
  base.Add("schemaVersion", 1);
  base.Add("chemistry", "PureWater");
  base.Add("prefix", "");
  base.Add("runs", DataNode::MakeArray());

  const std::vector<DataNode> dumps = {
      ResultsIndex::MakeDumpEntry("", "", "t1", 2, {RunEntry(0, 2, 1)}),
      ResultsIndex::MakeDumpEntry("", "sub_01", "t2", 2, {RunEntry(1, 2, 2)})};
  const DataNode merged = ResultsIndex::Build(dumps, &base, "/abs/results");

  // Still the per-dump manifest ...
  assert(Find(merged, "chemistry")->GetString() == "PureWater");
  assert(Find(merged, "runs") != nullptr);
  assert(Find(merged, "kind") == nullptr);
  // ... and now also the index.
  assert(Find(merged, "dumps")->GetElements().size() == 2);
  // The base itself is untouched.
  assert(Find(base, "dumps") == nullptr);

  // Rebuilding with more dumps replaces the array, it does not duplicate it.
  std::vector<DataNode> more = dumps;
  more.push_back(ResultsIndex::MakeDumpEntry("EndOfRun_", "", "t3", 1, {}));
  const DataNode again = ResultsIndex::Build(more, &base, "/abs/results");
  assert(Find(again, "dumps")->GetElements().size() == 3);
  int count = 0;
  for (const auto &member : again.GetMembers())
    if (member.first == "dumps")
      ++count;
  assert(count == 1);
}

static void TestEmptyIndex()
{
  const DataNode index = ResultsIndex::Build({}, nullptr, "");
  assert(Find(index, "dumps")->GetKind() == DataNode::Kind::Array);
  assert(Find(index, "dumps")->GetElements().empty());
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestDumpEntryForSubfolder();
  TestDumpEntryForPrefix();
  TestDumpEntrySkipsMissingBeamAndNonObjectRuns();
  TestStandaloneIndexListsEveryDumpInOrder();
  TestIndexOnTopOfFlatManifestKeepsBothValid();
  TestEmptyIndex();

  std::cout << "ResultsIndexTest: all tests passed\n";
  return 0;
}
