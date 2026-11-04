/// \file PreChemicalFilesTest.cc
/// Dependencies: Standard-library-only source and portable project code; current CMake target links
/// Geant4 without initializing its kernel. Optional MSVC CRT diagnostics.
/// Thread safety: Sequential test process; no concurrent execution within this test. Uses a fixed
/// temporary directory; do not run duplicate instances.
/// \brief Plain-assert unit tests for PreChemicalFiles (no test framework, no
/// Geant4 runtime -- exercises file-name parsing and filesystem staging moves).

#include "scoring/PreChemicalFiles.hh"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;

namespace
{
  fs::path TestRoot()
  {
    return fs::temp_directory_path() / "PreChemicalFilesTest";
  }

  void ResetTestRoot()
  {
    std::error_code ec;
    fs::remove_all(TestRoot(), ec);
    fs::create_directories(TestRoot(), ec);
  }

  void WriteFile(const fs::path& path, const std::string& content)
  {
    std::ofstream(path.string()) << content;
  }

  std::string ReadFile(const fs::path& path)
  {
    std::ifstream in(path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  size_t CountEntries(const fs::path& dir)
  {
    size_t n = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
      (void)e;
      ++n;
    }
    return n;
  }

  // Resolver: <root>/dump/p_<name>
  std::function<std::string(const std::string&)> DumpResolver()
  {
    return [](const std::string& name) { return (TestRoot() / "dump" / ("p_" + name)).string(); };
  }
}

static void TestStagedFileNameFormat()
{
  assert(PreChemicalFiles::StagedFileName(0, 12) == "PreChemical_run0_event12.txt");
}

static void TestPerFormatNamesAndOrdering()
{
  assert(PreChemicalFiles::StagedFileName(1, 2, "csv") == "PreChemical_run1_event2.csv");
  assert(PreChemicalFiles::StagedFileName(1, 2, "txt") == PreChemicalFiles::StagedFileName(1, 2));
  assert(PreChemicalFiles::StagedTableFileName("h5") == "PreChemical.h5");
  assert(PreChemicalFiles::StagedTableFileName("root") == "PreChemical.root");
  assert(PreChemicalFiles::IsStagedTableFileName("PreChemical.h5"));
  assert(PreChemicalFiles::IsStagedTableFileName("PreChemical.root"));
  assert(!PreChemicalFiles::IsStagedTableFileName("PreChemical.csv"));

  int run = -1, event = -1;
  std::string ext;
  assert(PreChemicalFiles::ParseStagedFileName("PreChemical_run3_event4.csv", run, event, ext));
  assert(run == 3 && event == 4 && ext == "csv");
  assert(!PreChemicalFiles::ParseStagedFileName("PreChemical_run3_event4.h5", run, event, ext));

  ResetTestRoot();
  const fs::path staging = TestRoot() / "staging";
  fs::create_directories(staging);
  for (const char* name : {"PreChemical.root", "PreChemical_run0_event10.csv", "PreChemical.h5",
                           "PreChemical_run0_event2.csv", "PreChemical_run0_event2.txt",
                           "PreChemical_run0_event10.txt"})
    WriteFile(staging / name, "x");
  const auto result = PreChemicalFiles::MoveStaged(staging.string(), DumpResolver());
  const std::vector<std::string> expected = {
    "p_PreChemical_run0_event2.txt",  "p_PreChemical_run0_event2.csv",
    "p_PreChemical_run0_event10.txt", "p_PreChemical_run0_event10.csv",
    "p_PreChemical.h5",               "p_PreChemical.root"};
  assert(result.moved == expected);
  assert(result.failures.empty());
  assert(CountEntries(staging) == 0);
}

static void TestParseRoundTrip()
{
  int run = -1;
  int event = -1;
  assert(PreChemicalFiles::ParseStagedFileName(PreChemicalFiles::StagedFileName(0, 12), run, event));
  assert(run == 0);
  assert(event == 12);

  assert(!PreChemicalFiles::ParseStagedFileName("PreChemical_run0_event12.txt.bak", run, event));
  assert(!PreChemicalFiles::ParseStagedFileName("PreChemical_runX_event1.txt", run, event));
  assert(!PreChemicalFiles::ParseStagedFileName("notes.txt", run, event));
}

static void TestStagingDirJoin()
{
  assert(PreChemicalFiles::StagingDir("out") == "out/.pending_prechem");
  assert(PreChemicalFiles::StagingDir("") == ".pending_prechem");
}

static void TestEnsureStagingDirCreates()
{
  ResetTestRoot();
  std::string err;
  const std::string base = (TestRoot() / "ensure").string();
  assert(PreChemicalFiles::EnsureStagingDir(base, err));
  assert(fs::is_directory(PreChemicalFiles::StagingDir(base)));
  assert(PreChemicalFiles::EnsureStagingDir(base, err)); // idempotent
}

static void TestMoveStagedOrdersNumerically()
{
  ResetTestRoot();
  const fs::path staging = TestRoot() / "staging";
  fs::create_directories(staging);
  WriteFile(staging / PreChemicalFiles::StagedFileName(0, 10), "b");
  WriteFile(staging / PreChemicalFiles::StagedFileName(1, 0), "c");
  WriteFile(staging / PreChemicalFiles::StagedFileName(0, 2), "a");

  const auto result = PreChemicalFiles::MoveStaged(staging.string(), DumpResolver());
  const std::vector<std::string> expected = {"p_PreChemical_run0_event2.txt",
                                             "p_PreChemical_run0_event10.txt",
                                             "p_PreChemical_run1_event0.txt"};
  assert(result.moved == expected);
  assert(result.failures.empty());
  assert(CountEntries(staging) == 0);
  assert(ReadFile(TestRoot() / "dump" / "p_PreChemical_run0_event2.txt") == "a");
}

static void TestMoveStagedOverwritesTarget()
{
  ResetTestRoot();
  const fs::path staging = TestRoot() / "staging";
  fs::create_directories(staging);
  fs::create_directories(TestRoot() / "dump");
  const fs::path target = TestRoot() / "dump" / "p_PreChemical_run0_event0.txt";
  WriteFile(target, "old");
  WriteFile(staging / PreChemicalFiles::StagedFileName(0, 0), "new");

  const auto result = PreChemicalFiles::MoveStaged(staging.string(), DumpResolver());
  assert(result.failures.empty());
  assert(result.moved.size() == 1);
  assert(ReadFile(target) == "new");
}

static void TestMoveStagedIgnoresForeignFiles()
{
  ResetTestRoot();
  const fs::path staging = TestRoot() / "staging";
  fs::create_directories(staging);
  WriteFile(staging / "notes.txt", "keep");
  WriteFile(staging / PreChemicalFiles::StagedFileName(0, 0), "x");

  const auto result = PreChemicalFiles::MoveStaged(staging.string(), DumpResolver());
  assert(result.moved.size() == 1);
  assert(result.failures.empty());
  assert(fs::exists(staging / "notes.txt"));
}

static void TestMoveStagedMissingDirIsEmpty()
{
  ResetTestRoot();
  const auto result =
    PreChemicalFiles::MoveStaged((TestRoot() / "does-not-exist").string(), DumpResolver());
  assert(result.moved.empty());
  assert(result.failures.empty());
}

static void TestMoveStagedKeepsEmptyFile()
{
  ResetTestRoot();
  const fs::path staging = TestRoot() / "staging";
  fs::create_directories(staging);
  WriteFile(staging / PreChemicalFiles::StagedFileName(0, 3), "");

  const auto result = PreChemicalFiles::MoveStaged(staging.string(), DumpResolver());
  assert(result.moved.size() == 1);
  const fs::path target = TestRoot() / "dump" / "p_PreChemical_run0_event3.txt";
  assert(fs::exists(target));
  assert(fs::file_size(target) == 0);
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  TestStagedFileNameFormat();
  TestParseRoundTrip();
  TestPerFormatNamesAndOrdering();
  TestStagingDirJoin();
  TestEnsureStagingDirCreates();
  TestMoveStagedOrdersNumerically();
  TestMoveStagedOverwritesTarget();
  TestMoveStagedIgnoresForeignFiles();
  TestMoveStagedMissingDirIsEmpty();
  TestMoveStagedKeepsEmptyFile();

  std::error_code ec;
  fs::remove_all(TestRoot(), ec);

  std::cout << "All PreChemicalFiles tests passed." << std::endl;
  return 0;
}
