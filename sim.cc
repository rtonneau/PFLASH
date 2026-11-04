/// \file sim.cc
/// Sets up and runs homogeneous-water radiolysis from a macro.
/// Dependencies: Geant4 and project services; executable lookup also uses OS APIs.
/// Thread safety: Main configures shared services before workers; shutdown follows the run manager.
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
#include "scoring/OutputFormatsMessenger.hh"
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

// Directory holding the executable; empty on a reported OS lookup failure.
// Filesystem conversion/allocation exceptions propagate.
static std::string ExecutableDir()
{
  namespace fs = std::filesystem;
  std::error_code error;
#ifdef _WIN32
  std::vector<wchar_t> executablePathBuffer(32768);
  DWORD pathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(),
                                      static_cast<DWORD>(executablePathBuffer.size()));
  if (pathLength == 0 || pathLength >= executablePathBuffer.size()) return "";
  return fs::path(std::wstring(executablePathBuffer.data(), pathLength)).parent_path().string();
#else
  fs::path executablePath = fs::read_symlink("/proc/self/exe", error);
  if (error) return "";
  return executablePath.parent_path().string();
#endif
}

// Set the lazy root only when executable lookup succeeded; OutputDir retains its fallback.
static void ConfigureDefaultOutputDir()
{
  const std::string exeDir = ExecutableDir();
  if (!exeDir.empty())
    OutputDir::SetDefaultDir((std::filesystem::path(exeDir) / "results").string());
}

static const G4bool kUseGui = false;
std::ofstream out;

// Explicit startup seed; /random/setSeeds may override it before /run/beamOn.
// This does not make the full simulation bit-reproducible: compare outputs
// statistically, including successive events that share a random engine.
constexpr long kDefaultSeed = 12345;

// Register options without parsing them or creating runtime resources.
static void RegisterArgumentFlags(ArgParser &argParser)
{
  argParser.AddIntFlag("--threads", [](G4int value, G4String &err) {
    G4int availableCores = G4Threading::G4GetNumberOfCores();
    if (value > availableCores)
    {
      err = "requested " + std::to_string(value) + " threads, but only "
            + std::to_string(availableCores) + " cores are available";
      return false;
    }
    return true;
  });
  // "--dir <path>" redirects all output files (Species.Txt, the CSV
  // ntuples, per-thread/per-event pre-chemical dumps, and the
  // /chem/reaction/dump target) into <path> instead of the lazy results root.
  // <path> itself is created if missing; its parent must already exist.
  argParser.AddStringFlag("--dir");
}

int main(int argc, char **argv)
{
  DnaLogger::SetLevel(DnaLogger::Level::Quiet);
  RunManifest::MarkProcessStart();

  G4Random::setTheSeed(kDefaultSeed);

  // Keep elapsed wall timing through the final output flush.
  G4Timer *simulationTimer = new G4Timer();
  simulationTimer->Start();

  // Serial is the default; --threads selects an MT run manager. Logging stays
  // Quiet until the macro configures it, or a CLI failure enables Error output.
  ArgParser argParser;
  RegisterArgumentFlags(argParser);
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
  // Prefer <exeDir>/results when no explicit root is configured. Creation is
  // lazy so /run/outputDir can replace it; lookup/creation failure falls back to cwd.
  ConfigureDefaultOutputDir();
  G4int requestedThreads = argParser.GetInt("--threads");
  G4RunManagerType runManagerType =
      (requestedThreads > 0) ? G4RunManagerType::MT : G4RunManagerType::Serial;

  G4RunManager *runManager = G4RunManagerFactory::CreateRunManager(runManagerType);

  // When MT is selected, a macro can still override its worker count with
  // /run/numberOfThreads before /run/initialize.
  if (!kUseGui && runManagerType != G4RunManagerType::Serial)
    runManager->SetNumberOfThreads(requestedThreads);

  G4UImanager *uiManager = G4UImanager::GetUIpointer();

  // Exposes "/dnaLogger/verbose <level>" so the logging level can be set from a macro
  DnaLoggerMessenger *dnaLoggerMessenger = new DnaLoggerMessenger();
  // Exposes "/run/outputDir <path>" as a macro-file counterpart to --dir
  OutputDirMessenger *outputDirMessenger = new OutputDirMessenger();
  // Exposes "/output/defaultFormat" and "/output/<product>/format" to choose the output file formats
  OutputFormatsMessenger *outputFormatsMessenger = new OutputFormatsMessenger();
  // Exposes "/run/dumpDataAndReset [prefix]" to flush accumulated
  // species/reaction/interaction/energy data to disk and reset it
  RunAccumulatorMessenger *runAccumulatorMessenger = new RunAccumulatorMessenger();

  DetectorConstruction *detector = new DetectorConstruction();
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new PhysicsList);
  runManager->SetUserInitialization(new ActionInitialization());

  G4String storage = ".";

  if (kUseGui)
  {
    // ----------------------
    // - GUI mode execution
    // ----------------------
    runManager->SetNumberOfThreads(1);
    runManager->Initialize();
    G4cout << "starting GUI" << G4endl;
    G4VisManager *visManager = new G4VisExecutive;
    visManager->Initialize();

    G4UIExecutive *ui = new G4UIExecutive(argc, argv);
    uiManager->ApplyCommand("/control/execute macro/vis.mac");
    ui->SessionStart();

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
      for (const auto &candidatePath : resolved.tried) G4cerr << "  " << candidatePath << G4endl;
      return 1;
    }
    G4String macroFile = resolved.path;
    G4cout << "starting batch mode with macro file: " << macroFile << G4endl;
    RunManifest::SetMacroName(macroFile);
    // The macro owns /run/initialize so its PreInit configuration takes effect.

    uiManager->ApplyCommand("/control/execute " + macroFile);
  }

  // Safety net: flush any accumulated data that was never explicitly
  // dumped via /run/dumpDataAndReset, so it isn't silently lost.
  runAccumulatorMessenger->FlushIfPending("EndOfRun_");

  simulationTimer->Stop();

  G4cout << "The simulation took: " << simulationTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  delete simulationTimer;
  delete dnaLoggerMessenger;
  delete outputDirMessenger;
  delete outputFormatsMessenger;
  delete runAccumulatorMessenger;
  delete runManager;

  return 0;
}
