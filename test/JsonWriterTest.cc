/// \file JsonWriterTest.cc
/// \brief Plain-assert unit tests for DataNode + JsonWriter (no test
/// framework, no Geant4 runtime -- exercises the pure tree and serialiser).

#include "scoring/DataNode.hh"
#include "scoring/JsonWriter.hh"

#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static std::string Render(const DataNode &node)
{
  std::ostringstream stream;
  JsonWriter::Write(stream, node);
  return stream.str();
}

static void TestEscapeJson()
{
  assert(JsonWriter::EscapeJson("a\"b\\c\nd\te") == "a\\\"b\\\\c\\nd\\te");
  assert(JsonWriter::EscapeJson(std::string(1, '\x01')) == "\\u0001");
}

static void TestScalars()
{
  assert(Render(DataNode()) == "null\n");
  assert(Render(DataNode(true)) == "true\n");
  assert(Render(DataNode(false)) == "false\n");
  assert(Render(DataNode(4)) == "4\n");
  assert(Render(DataNode(12345678901LL)) == "12345678901\n");
  assert(Render(DataNode(10.)) == "10\n");
  assert(Render(DataNode(0.000273)) == "0.000273\n");
  assert(Render(DataNode(std::numeric_limits<double>::quiet_NaN())) == "null\n");
  assert(Render(DataNode(std::numeric_limits<double>::infinity())) == "null\n");
  assert(Render(DataNode("C:\\a\"b")) == "\"C:\\\\a\\\"b\"\n");
}

static void TestEmptyContainers()
{
  assert(Render(DataNode::MakeArray()) == "[]\n");
  assert(Render(DataNode::MakeObject()) == "{}\n");
}

static void TestScalarOnlyContainersInline()
{
  DataNode scavenger = DataNode::MakeObject();
  scavenger.Add("species", "O2").Add("molarity_M", 0.000273);
  assert(Render(scavenger) == "{\"species\": \"O2\", \"molarity_M\": 0.000273}\n");

  DataNode direction = DataNode::MakeArray();
  direction.Push(0).Push(0).Push(1);
  assert(Render(direction) == "[0, 0, 1]\n");
}

static void TestNestedContainersExpand()
{
  DataNode inner = DataNode::MakeObject();
  inner.Add("x", 1);
  DataNode list = DataNode::MakeArray();
  list.Push(inner);
  DataNode vec = DataNode::MakeArray();
  vec.Push(0).Push(0).Push(1);

  DataNode root = DataNode::MakeObject();
  root.Add("a", 1).Add("v", vec).Add("s", list);
  assert(Render(root) == "{\n  \"a\": 1,\n  \"v\": [0, 0, 1],\n  \"s\": [\n    {\"x\": 1}\n  ]\n}\n");
}

static void TestAddReplacesInPlace()
{
  DataNode root = DataNode::MakeObject();
  root.Add("a", 1).Add("b", 2).Add("a", 3);
  assert(root.GetMembers().size() == 2);
  assert(Render(root) == "{\"a\": 3, \"b\": 2}\n");
}

static void TestNullBecomesContainer()
{
  DataNode object;
  object.Add("k", 1);
  assert(object.GetKind() == DataNode::Kind::Object);

  DataNode array;
  array.Push(1);
  assert(array.GetKind() == DataNode::Kind::Array);
}

static void TestKindMismatchThrows()
{
  bool thrown = false;
  try
  {
    DataNode(1).Add("k", 1);
  }
  catch (const std::logic_error &)
  {
    thrown = true;
  }
  assert(thrown);

  thrown = false;
  try
  {
    DataNode::MakeObject().Push(1);
  }
  catch (const std::logic_error &)
  {
    thrown = true;
  }
  assert(thrown);
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

  TestEscapeJson();
  TestScalars();
  TestEmptyContainers();
  TestScalarOnlyContainersInline();
  TestNestedContainersExpand();
  TestAddReplacesInPlace();
  TestNullBecomesContainer();
  TestKindMismatchThrows();

  std::cout << "JsonWriterTest: all tests passed" << std::endl;
  return 0;
}
