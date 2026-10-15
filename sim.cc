/// \file sim.cc
/// \brief Basic common implementation of water radiolysis for CONV and UHDR
// Created: 2026-08-19

#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "core/DnaLogger.hh"
#include "core/DnaLoggerMessenger.hh"
#include "core/ArgParser.hh"
#include "core/MacroResolver.hh"
#include "core/OutputDir.hh"
#include "core/OutputDirMessenger.hh"
#include "physics/PhysicsList.hh"
#include "scoring/RunAccumulatorMessenger.hh"
#include "scoring/RunManifest.hh"

#include "G4ScoringManager.hh"
#include "G4DNAChemistryManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4Threading.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4Timer.hh"
#include "Randomize.hh"

#include <cstdlib>
#include <filesystem>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include <time.h>

// Directory holding the running executable (empty if it cannot be determined).
static std::string ExecutableDir()
{
  namespace fs = std::filesystem;
  std::error_code ec;
#ifdef _WIN32
  std::vector<wchar_t> buf(32768);
  DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
  if (n == 0 || n >= buf.size()) return "";
  return fs::path(std::wstring(buf.data(), n)).parent_path().string();
#else
  fs::path exe = fs::read_symlink("/proc/self/exe", ec);
  if (ec) return "";
  return exe.parent_path().string();
#endif
}

static const G4bool useGUI = false;
std::ofstream out;

// Makes the RNG seed an explicit, documented property instead of an
// undocumented CLHEP default. This reproduces the pre-chemistry physics
// stage exactly (verified: identical track lists/counts across separate
// process launches) but does NOT make the full pipeline reproducible --
// the chemistry (IT) stepping stage still diverges run-to-run even with
// this seed fixed; see .scratch/geant4-testing/spec.md addendum and
// ticket 02 for the investigation. A macro's "/random/setSeeds <a> <b>"
// overrides this before "/run/initialize" runs.
constexpr long kDefaultSeed = 12345;

int main(int argc, char **argv)
{
  DnaLogger::SetLevel(DnaLogger::Level::Quiet);
  RunManifest::MarkProcessStart();

  G4Random::setTheSeed(kDefaultSeed);

  // Instantiate the G4Timer object, to monitor the CPU time spent for
  // the entire execution
  G4Timer *theTimer = new G4Timer();
  theTimer->Start();

  // Default is Serial (single-threaded, reproducible). Pass "--threads N" on
  // the command line to run multithreaded with N worker threads instead.
  // DnaLogger level is forced to Error first since it otherwise defaults to
  // Quiet before "/dnaLogger/verbose" can be applied.
  ArgParser argParser;
  argParser.AddIntFlag("--threads", [](G4int value, G4String &err) {
    G4int available = G4Threading::G4GetNumberOfCores();
    if (value > available)
    {
      err = "requested " + std::to_string(value) + " threads, but only "
            + std::to_string(available) + " cores are available";
      return false;
    }
    return true;
  });
  // "--dir <path>" redirects all output files (Species.Txt, the CSV
  // ntuples, per-thread/per-event pre-chemical dumps, and the
  // /chem/reaction/dump target) into <path> instead of cwd. <path> itself
  // is created if missing; its parent must already exist.
  argParser.AddStringFlag("--dir");
  if (!argParser.Parse(argc, argv))
  {
    DnaLogger::SetLevel(DnaLogger::Level::Error);
    DnaLogger::Print(DnaLogger::Level::Error, argParser.GetError());
    exit(1);
  }
  G4String outputDirError;
  if (!OutputDir::Configure(argParser.GetString("--dir"), outputDirError))
  {
    DnaLogger::SetLevel(DnaLogger::Level::Error);
    DnaLogger::Print(DnaLogger::Level::Error, outputDirError);
    exit(1);
  }
  // With neither --dir nor /run/outputDir, output goes to <exeDir>/results.
  // The default is applied lazily (created at the first output), so
  // "/run/outputDir" in the macro still replaces it.
  {
    const std::string exeDir = ExecutableDir();
    if (!exeDir.empty())
      OutputDir::SetDefaultDir((std::filesystem::path(exeDir) / "results").string());
  }
  G4int requestedThreads = argParser.GetInt("--threads");
  G4RunManagerType runManagerType =
      (requestedThreads > 0) ? G4RunManagerType::MT : G4RunManagerType::Serial;

  G4RunManager *runManager = G4RunManagerFactory::CreateRunManager(runManagerType);

  // A macro may still override with "/run/numberOfThreads N" before
  // "/run/initialize".
  if (!useGUI && runManagerType != G4RunManagerType::Serial)
    runManager->SetNumberOfThreads(requestedThreads);

  G4UImanager *UIManager = G4UImanager::GetUIpointer();

  // Exposes "/dnaLogger/verbose <level>" so the logging level can be set from a macro
  DnaLoggerMessenger *dnaLoggerMessenger = new DnaLoggerMessenger();
  // Exposes "/run/outputDir <path>" as a macro-file counterpart to --dir
  OutputDirMessenger *outputDirMessenger = new OutputDirMessenger();
  // Exposes "/run/dumpDataAndReset [prefix]" to flush accumulated
  // species/reaction/interaction/energy data to disk and reset it
  RunAccumulatorMessenger *runAccumulatorMessenger = new RunAccumulatorMessenger();

  //////////
  // Set mandatory user initialization classes
  //
  DetectorConstruction *detector = new DetectorConstruction();
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList);
  runManager->SetUserInitialization(new ActionInitialization());

  G4String storage = ".";

  // toml->SetRootPath(storage);
  // Start Simulations
  if (useGUI)
  {
    // ----------------------
    // - GUI mode execution
    // ----------------------
    runManager->SetNumberOfThreads(1);
    runManager->Initialize();
    G4cout << "starting GUI" << G4endl;
    // Initialize visualization
    G4VisManager *visManager = new G4VisExecutive;
    visManager->Initialize();

    // Start interactive session
    G4UIExecutive *ui = new G4UIExecutive(argc, argv);
    // UIManager->ApplyCommand("/vis/open OGL");
    UIManager->ApplyCommand("/control/execute macro/vis.mac");
    ui->SessionStart();

    // Clean up GUI resources
    delete ui;
    delete visManager;
  }
  else
  {
    // ------------------------
    // - Batch mode execution
    // ------------------------
    const std::string macroArg = (argc > 1) ? std::string(argv[1]) : std::string("beam.in");
    const MacroResolver::Result resolved = MacroResolver::Resolve(macroArg, ExecutableDir());
    if (!resolved.found)
    {
      G4cerr << "Macro file '" << macroArg << "' not found. Tried:" << G4endl;
      for (const auto &t : resolved.tried) G4cerr << "  " << t << G4endl;
      return 1;
    }
    G4String macroFile = resolved.path;
    G4cout << "starting batch mode with macro file: " << macroFile << G4endl;
    RunManifest::SetMacroName(macroFile);
    // Batch mode execution
    // rem: Initialize is performed in beam.in macro!

    UIManager->ApplyCommand("/control/execute " + macroFile);
  }

  // Safety net: flush any accumulated data that was never explicitly
  // dumped via /run/dumpDataAndReset, so it isn't silently lost.
  runAccumulatorMessenger->FlushIfPending("EndOfRun_");

  // Stop the benchmark here
  theTimer->Stop();

  G4cout << "The simulation took: " << theTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  // Clean up
  delete theTimer;
  delete dnaLoggerMessenger;
  delete outputDirMessenger;
  delete runAccumulatorMessenger;
  delete runManager;

  return 0;
}
