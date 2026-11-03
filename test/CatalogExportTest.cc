/// \file CatalogExportTest.cc
/// Dependencies: CatalogExport tree builders and JsonWriter; no Geant4 kernel initialization.
/// Thread safety: Sequential test process.
/// \brief Plain-assert unit tests for the CatalogExport::Make* node builders.

#include "chemistry/CatalogExport.hh"
#include "scoring/JsonWriter.hh"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static std::string Json(const DataNode& node)
{
  std::ostringstream out;
  JsonWriter::Write(out, node);
  return out.str();
}

static bool Before(const std::string& text, const std::string& a, const std::string& b)
{
  const auto pa = text.find(a);
  const auto pb = text.find(b);
  return pa != std::string::npos && pb != std::string::npos && pa < pb;
}

static void TestReactionNode()
{
  const std::string oh = "\xC2\xB0OH";
  const std::string json = Json(CatalogExport::MakeReactionNode("e_aq", oh, {"OHm"}, 2.95e10, 2, 1, true));
  assert(json.find("\"type\": 1") != std::string::npos);
  assert(json.find("\"k_M-1s-1\": 29500000000") != std::string::npos);
  assert(json.find(oh) != std::string::npos);
  assert(json.find("\"mesoOnly\": true") != std::string::npos);
  assert(Before(json, "reactants", "products"));
  assert(json.find("\"order\": 2") != std::string::npos);
  assert(json.find("k_s-1") == std::string::npos);
  assert(Before(json, "products", "\"order\""));
  assert(Before(json, "\"order\"", "k_M-1s-1"));
}

// H2O-partner reactions are first order: rate in s^-1 under "k_s-1", no "k_M-1s-1".
static void TestFirstOrderReactionNode()
{
  assert(CatalogExport::RateKey(1) == "k_s-1");
  assert(CatalogExport::RateKey(2) == "k_M-1s-1");
  const std::string json =
    Json(CatalogExport::MakeReactionNode("e_aq", "H2O", {"H", "OHm(B)"}, 868.21, 1, 0, true));
  assert(json.find("\"order\": 1") != std::string::npos);
  assert(json.find("\"k_s-1\": 868.21") != std::string::npos);
  assert(json.find("k_M-1s-1") == std::string::npos);
}

static void TestEmptyProducts()
{
  const std::string json = Json(CatalogExport::MakeReactionNode("H3Op", "OHm", {}, 1.43e11, 2, 0, false));
  assert(json.find("\"products\": []") != std::string::npos);
  assert(json.find("\"mesoOnly\": false") != std::string::npos);
}

static void TestBulkNode()
{
  const std::string json = Json(CatalogExport::MakeBulkNode("e_aq", "O2", {"O2m"}, 1.74e10, 2));
  assert(Before(json, "\"reactant\"", "\"partner\""));
  assert(Before(json, "\"partner\"", "\"products\""));
  assert(Before(json, "\"products\"", "\"order\": 2"));
  assert(Before(json, "\"order\": 2", "\"k_M-1s-1\""));

  const std::string water =
    Json(CatalogExport::MakeBulkNode("e_aq", "H2O", {"H", "OHm(B)"}, 868.21, 1));
  assert(water.find("\"order\": 1") != std::string::npos);
  assert(water.find("\"k_s-1\": 868.21") != std::string::npos);
  assert(water.find("k_M-1s-1") == std::string::npos);
}

static void TestMoleculeNode()
{
  const std::string json = Json(CatalogExport::MakeMoleculeNode("H2O2", 2.3e-9, 0.21, 0, "H2O2^0"));
  assert(Before(json, "\"name\"", "\"D_m2s-1\""));
  assert(Before(json, "\"D_m2s-1\"", "\"radius_nm\""));
  assert(Before(json, "\"radius_nm\"", "\"charge\""));
  assert(Before(json, "\"charge\"", "\"configuration\": \"H2O2^0\""));
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestReactionNode();
  TestFirstOrderReactionNode();
  TestEmptyProducts();
  TestBulkNode();
  TestMoleculeNode();
  std::cout << "CatalogExportTest: all tests passed\n";
  return 0;
}
