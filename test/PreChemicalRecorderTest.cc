/// \file PreChemicalRecorderTest.cc
/// Dependencies: PreChemicalRecorder and portable project code; links Geant4 for G4VPhysChemIO
/// without initializing its kernel. Rows are fed directly (no G4Track).
/// Thread safety: sequential test process; uses a fixed temporary directory.
/// \brief Plain-assert unit tests for PreChemicalRecorder.

#include "scoring/PreChemicalFiles.hh"
#include "scoring/PreChemicalRecorder.hh"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;
using OutputFormats::Format;

namespace
{
  fs::path TestRoot() { return fs::temp_directory_path() / "PreChemicalRecorderTest"; }

  std::string ReadFile(const fs::path& path)
  {
    std::ifstream in(path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  void Reset()
  {
    std::error_code ec;
    fs::remove_all(TestRoot(), ec);
    fs::create_directories(TestRoot(), ec);
    PreChemicalRecorder::TakeStaged();
  }
}

static void TestTableContent()
{
  PreChemicalRecorder recorder(2, 5);
  recorder.AddWaterMolecule(1, 0, 3, 12.5, 1.0, 2.0, 3.0);
  recorder.AddWaterMolecule(4, 1, 0, 8.25, -1.0, 0.0, 0.5);
  recorder.AddSolvatedElectron(1, 1.5, 10.0, 20.0, 30.0, true, 11.0, 21.0, 31.0);
  recorder.AddSolvatedElectron(7, 0.5, 4.0, 5.0, 6.0, false, 99.0, 99.0, 99.0);

  const OutputTable& t = recorder.Table();
  assert(t.Columns().size() == 13);
  const char* names[] = {"run", "event", "parentTrackId", "molecule", "modification", "level",
                         "energy_eV", "parentX_nm", "parentY_nm", "parentZ_nm", "x_nm", "y_nm",
                         "z_nm"};
  for (std::size_t i = 0; i < 13; ++i) assert(t.Columns()[i].name == names[i]);
  assert(t.Rows().size() == 4);

  const auto& r0 = t.Rows()[0];
  assert(std::get<std::int64_t>(r0[0]) == 2 && std::get<std::int64_t>(r0[1]) == 5);
  assert(std::get<std::int64_t>(r0[2]) == 1);
  assert(std::get<std::string>(r0[3]) == "H2O" && std::get<std::string>(r0[4]) == "ionisation");
  assert(std::get<std::int64_t>(r0[5]) == 3 && std::get<double>(r0[6]) == 12.5);
  assert(std::get<double>(r0[7]) == 1.0 && std::get<double>(r0[10]) == 1.0);

  assert(std::get<std::string>(t.Rows()[1][4]) == "excitation");

  const auto& r2 = t.Rows()[2];
  assert(std::get<std::string>(r2[3]) == "e_aq" && std::get<std::string>(r2[4]) == "solvation");
  assert(std::get<std::int64_t>(r2[5]) == -1);
  assert(std::get<double>(r2[7]) == 10.0 && std::get<double>(r2[10]) == 11.0);

  const auto& r3 = t.Rows()[3];
  assert(std::get<double>(r3[10]) == 4.0 && std::get<double>(r3[12]) == 6.0);

  PreChemicalRecorder other(0, 0);
  other.AddWaterMolecule(1, 2, 0, 5.0, 0, 0, 0);
  other.AddWaterMolecule(1, 9, 0, 5.0, 0, 0, 0);
  assert(std::get<std::string>(other.Table().Rows()[0][4]) == "dissociativeAttachment");
  assert(std::get<std::string>(other.Table().Rows()[1][4]) == "unknown");
}

static void TestPerEventFiles()
{
  Reset();
  const std::string staging = (TestRoot() / "staging").string();
  std::error_code ec;
  fs::create_directories(staging, ec);

  PreChemicalRecorder withRows(0, 1);
  withRows.AddWaterMolecule(1, 0, 1, 10.0, 0, 0, 0);
  std::string err;
  assert(withRows.EndEvent(staging, {Format::Txt, Format::Csv}, err));
  PreChemicalRecorder empty(0, 2);
  assert(empty.EndEvent(staging, {Format::Txt, Format::Csv}, err));

  assert(fs::exists(fs::path(staging) / "PreChemical_run0_event1.txt"));
  assert(fs::exists(fs::path(staging) / "PreChemical_run0_event1.csv"));
  assert(fs::exists(fs::path(staging) / "PreChemical_run0_event2.csv"));
  const std::string csv = ReadFile(fs::path(staging) / "PreChemical_run0_event1.csv");
  assert(csv.rfind("run,event,parentTrackId,molecule,modification,level,energy_eV", 0) == 0);
  assert(csv.find("H2O,ionisation,1,10") != std::string::npos);
  // Empty event: header only.
  const std::string emptyCsv = ReadFile(fs::path(staging) / "PreChemical_run0_event2.csv");
  assert(emptyCsv.find('\n') == emptyCsv.size() - 1);
  assert(PreChemicalRecorder::StagedRowCount() == 0);
}

static void TestTableAppendOnce()
{
  Reset();
  const std::string staging = (TestRoot() / "staging").string();
  PreChemicalRecorder a(0, 0);
  a.AddWaterMolecule(1, 0, 1, 10.0, 0, 0, 0);
  a.AddSolvatedElectron(1, 1.0, 0, 0, 0, false, 0, 0, 0);
  std::string err;
  // h5 and root together stage the rows once.
  assert(a.EndEvent(staging, {Format::H5, Format::Root}, err));
  assert(PreChemicalRecorder::StagedRowCount() == 2);
  assert(a.EndEvent(staging, {Format::H5}, err)); // second call is a no-op
  assert(PreChemicalRecorder::StagedRowCount() == 2);

  PreChemicalRecorder b(0, 1);
  b.AddWaterMolecule(2, 1, 2, 7.0, 0, 0, 0);
  assert(b.EndEvent(staging, {Format::H5}, err));
  assert(PreChemicalRecorder::StagedRowCount() == 3);

  const OutputTable taken = PreChemicalRecorder::TakeStaged();
  assert(taken.Rows().size() == 3);
  assert(PreChemicalRecorder::StagedRowCount() == 0);
  assert(!fs::exists(fs::path(staging) / "PreChemical_run0_event0.csv"));
}

static void TestNoFormatsStagesNothing()
{
  Reset();
  const std::string staging = (TestRoot() / "staging").string();
  PreChemicalRecorder a(0, 0);
  a.AddWaterMolecule(1, 0, 1, 10.0, 0, 0, 0);
  std::string err;
  assert(a.EndEvent(staging, {}, err));
  assert(PreChemicalRecorder::StagedRowCount() == 0);
  assert(!fs::exists(staging));
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestTableContent();
  TestPerEventFiles();
  TestTableAppendOnce();
  TestNoFormatsStagesNothing();
  std::error_code ec;
  fs::remove_all(TestRoot(), ec);
  std::cout << "All PreChemicalRecorder tests passed." << std::endl;
  return 0;
}
