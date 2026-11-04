/// \file ProductOutputTest.cc
/// Dependencies: Geant4 types/libraries, OutputDir and the table writers; no kernel initialization.
/// Thread safety: Sequential test process; uses a fixed temporary directory.
/// \brief Plain-assert tests for ProductOutput file naming (csv/txt multi-table, prefix, subdir).

#include "scoring/ProductOutput.hh"

#include "core/OutputDir.hh"
#include "scoring/OutputFormats.hh"
#include "scoring/OutputTable.hh"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;
using OutputFormats::Format;
using OutputFormats::Product;

namespace
{
  OutputTable MakeTable(const std::string &name)
  {
    OutputTable table(name, name + " table");
    table.AddColumn("x", "nm", "a value", OutputTable::ColumnType::Double);
    table.AddRow({1.5});
    return table;
  }

  bool Has(const std::vector<std::string> &files, const std::string &name)
  {
    return std::find(files.begin(), files.end(), name) != files.end();
  }
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  const fs::path root = fs::temp_directory_path() / "ProductOutputTest";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  G4String err;
  assert(OutputDir::Configure(root.string().c_str(), err));

  // csv + txt, two tables, prefix and subdir.
  std::string perr;
  assert(OutputFormats::SetProduct(Product::Reactions, "csv txt", perr));
  assert(OutputDir::ConfigureSubdir("sub", err));
  OutputDir::SetPrefix("p_");
  std::vector<std::string> files;
  ProductOutput::Write(Product::Reactions, {MakeTable("reactions"), MakeTable("catalog")}, "p_", files);
  assert(files.size() == 4);
  for (const char *name : {"p_Reactions.csv", "p_Reactions.txt", "p_ReactionsCatalog.csv", "p_ReactionsCatalog.txt"})
  {
    assert(Has(files, name));
    assert(fs::exists(root / "sub" / name));
  }

  // Default: csv only, no prefix, no subdir.
  OutputFormats::ResetForTest();
  OutputDir::SetPrefix("");
  assert(OutputDir::ConfigureSubdir("", err));
  files.clear();
  ProductOutput::Write(Product::TrackLengths, {MakeTable("trackLengths")}, "", files);
  assert(files.size() == 1 && files[0] == "TrackLengths.csv");
  assert(fs::exists(root / "TrackLengths.csv"));

  // none: nothing written.
  assert(OutputFormats::SetProduct(Product::TrackLengths, "none", perr));
  files.clear();
  ProductOutput::Write(Product::TrackLengths, {MakeTable("trackLengths")}, "", files);
  assert(files.empty());

  // root (always available) and h5 (when built with HDF5): one file for all tables.
  assert(OutputFormats::SetProduct(Product::TrackLengths, "root", perr));
  files.clear();
  ProductOutput::Write(Product::TrackLengths, {MakeTable("trackLengths"), MakeTable("extra")}, "", files);
  assert(files.size() == 1 && files[0] == "TrackLengths.root");
  assert(fs::exists(root / "TrackLengths.root"));

  fs::remove_all(root, ec);
  std::cout << "ProductOutputTest: all tests passed\n";
  return 0;
}
