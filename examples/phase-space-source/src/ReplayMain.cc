// phsp-replay: stage 2 of the phase-space source, physics only. Replays the
// Phase-space histories of an ordered list of Phase-space files in a water box
// of the stage-1 box side, one history per event, reseeded per history
// (ADR 0015), and writes the stage-2 deposit of each history next to its
// stage-1 in-box deposit.
//
//   phsp-replay <macro> --output <dir> [--threads N] [--physics stage1|option2]
//
//   <macro>     Geant4 commands applied line by line (blank and '#' lines are
//               skipped; a failing command is an error): /phsp/source/addFile
//               (one or more, ordered) and /phsp/source/baseSeed before
//               /run/initialize, then any number of
//               `/phsp/source/first k` + `/run/beamOn N` pairs, each replaying
//               global histories k..k+N-1. /run/initialize is issued after the
//               macro if it did not.
//   --output    directory for Histories.csv and Manifest.json (created if
//               missing; existing files are replaced)
//   --threads   worker threads, 1..256 (default 1); always the MT run manager
//   --physics   stage1 (default: the stage-1 physics setup applied to the box)
//               or option2 (the G4EmDNAPhysics_option2 constructor); see
//               ReplayPhysicsList.hh
//
// Output: Histories.csv (one row per replayed history, sorted by global index,
// identical for any thread count or range split), Manifest.json, and a line
//   REPLAY_DONE histories=.. stage1MeanEv=.. stage1SeEv=.. stage2MeanEv=..
//   stage2SeEv=.. diffMeanEv=.. diffSeEv=.. z=.. ...
// where diff = stage 2 - stage 1 per history and z = diffMean / diffSe.
// Exit status: 0 success, 2 bad arguments, 3 macro or initialization failure,
// 1 failure during the run or while writing. A G4Exception (FatalException:
// duplicate file, range past the end, box-side mismatch, ...) aborts.
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <stdlib.h>
#endif

#include "DataNode.hh"
#include "JsonWriter.hh"
#include "PhaseSpaceFormat.hh"
#include "PhaseSpaceSource.hh"
#include "ReplayActions.hh"
#include "ReplayDetectorConstruction.hh"
#include "ReplayPhysicsList.hh"

#include "G4EmParameters.hh"
#include "G4ProductionCuts.hh"
#include "G4Region.hh"
#include "G4RegionStore.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4Version.hh"
#include "Randomize.hh"

namespace
{

constexpr int kMaxThreads = 256;
constexpr const char* kUsage =
    "usage: phsp-replay <macro> --output <dir> [--threads N] [--physics stage1|option2]";

struct Arguments
{
  std::filesystem::path macro;
  std::filesystem::path outputDir;
  int threads = 1;
  phsp::ReplayPhysics physics = phsp::ReplayPhysics::kStage1;
};

class ExitError : public std::runtime_error
{
 public:
  ExitError(int status, const std::string& message) : std::runtime_error(message), fStatus(status) {}
  int Status() const { return fStatus; }

 private:
  int fStatus;
};

std::uint64_t ParseUnsigned(const std::string& option, const std::string& text)
{
  if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
    throw ExitError(2, option + " needs an unsigned decimal integer, got '" + text + "'");
  try
  {
    return std::stoull(text, nullptr, 10);
  }
  catch (const std::exception&)
  {
    throw ExitError(2, option + " is out of range: '" + text + "'");
  }
}

Arguments ParseArguments(int argc, char** argv)
{
  Arguments arguments;
  bool haveMacro = false, haveOutput = false, haveThreads = false, havePhysics = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (option.rfind("--", 0) != 0)
    {
      if (haveMacro) throw ExitError(2, "more than one macro given ('" + option + "')");
      arguments.macro = option;
      haveMacro = true;
      continue;
    }
    if (i + 1 >= argc) throw ExitError(2, "missing value after " + option);
    const std::string value = argv[++i];
    if (option == "--output" && !haveOutput)
    {
      arguments.outputDir = value;
      haveOutput = true;
    }
    else if (option == "--threads" && !haveThreads)
    {
      const std::uint64_t threads = ParseUnsigned(option, value);
      if (threads < 1 || threads > kMaxThreads)
        throw ExitError(2, "--threads must be in 1.." + std::to_string(kMaxThreads));
      arguments.threads = static_cast<int>(threads);
      haveThreads = true;
    }
    else if (option == "--physics" && !havePhysics)
    {
      if (value == "stage1")
        arguments.physics = phsp::ReplayPhysics::kStage1;
      else if (value == "option2")
        arguments.physics = phsp::ReplayPhysics::kOption2;
      else
        throw ExitError(2, "--physics must be stage1 or option2, got '" + value + "'");
      havePhysics = true;
    }
    else
      throw ExitError(2, "unknown or repeated option " + option);
  }
  if (!haveMacro || !haveOutput) throw ExitError(2, kUsage);
  if (!std::filesystem::is_regular_file(arguments.macro))
    throw ExitError(2, "macro not found: " + arguments.macro.string());
  if (arguments.outputDir.empty()) throw ExitError(2, "--output needs a directory");
  return arguments;
}

std::string Trim(const std::string& text)
{
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

G4ApplicationState CurrentState()
{
  return G4StateManager::GetStateManager()->GetCurrentState();
}

// Applies the macro line by line; returns the commands applied.
std::vector<std::string> ApplyMacro(const std::filesystem::path& macro)
{
  std::ifstream in(macro);
  if (!in) throw ExitError(3, "cannot open macro " + macro.string());
  auto* ui = G4UImanager::GetUIpointer();
  std::vector<std::string> applied;
  std::string line;
  int lineNumber = 0;
  while (std::getline(in, line))
  {
    ++lineNumber;
    const auto command = Trim(line);
    if (command.empty() || command.front() == '#') continue;
    const auto where = macro.filename().string() + ":" + std::to_string(lineNumber);
    if (const int status = ui->ApplyCommand(command); status != 0)
      throw ExitError(3, where + ": command failed (G4UImanager code " + std::to_string(status) + "): " + command);
    applied.push_back(command);
  }
  if (in.bad()) throw ExitError(3, "error reading macro " + macro.string());
  return applied;
}

// Shortest text that reads back to the same double.
std::string Real(double value)
{
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
}

void WriteHistoriesCsv(const std::filesystem::path& path, const std::vector<phsp::ReplayHistoryRow>& rows)
{
  std::ofstream out(path, std::ios::trunc);
  if (!out) throw ExitError(1, "cannot write " + path.string());
  out << "globalIndex,fileId,historyIndex,stage1EventId,boxIx,boxIy,recordCount,stage1InBoxDepositEv,"
         "stage2DepositEv,tracksKilledOutsideBox\n";
  for (const auto& row : rows)
    out << row.globalIndex << ',' << phsp::FormatId(row.fileId) << ',' << row.historyIndex << ','
        << row.stage1EventId << ',' << row.boxIx << ',' << row.boxIy << ',' << row.recordCount << ','
        << Real(row.stage1DepositEv) << ',' << Real(row.stage2DepositEv) << ',' << row.tracksKilledOutsideBox
        << '\n';
  out.flush();
  if (!out) throw ExitError(1, "error writing " + path.string());
}

// Mean and standard error of the mean (sample standard deviation / sqrt(n)).
struct MeanSe
{
  double mean = 0;
  double se = 0;
};

template <typename Value>
MeanSe Statistics(const std::vector<phsp::ReplayHistoryRow>& rows, Value value)
{
  MeanSe result;
  const auto n = static_cast<double>(rows.size());
  if (rows.empty()) return result;
  double sum = 0;
  for (const auto& row : rows) sum += value(row);
  result.mean = sum / n;
  if (rows.size() < 2) return result;
  double squares = 0;
  for (const auto& row : rows) squares += (value(row) - result.mean) * (value(row) - result.mean);
  result.se = std::sqrt(squares / (n - 1) / n);
  return result;
}

DataNode Strings(const std::vector<std::string>& values)
{
  DataNode array = DataNode::MakeArray();
  for (const auto& value : values) array.Push(DataNode(value));
  return array;
}

// What the run manager and the physics list actually used (not the CLI values).
struct EffectiveSettings
{
  G4int threads = 0;
  G4double defaultCut = 0;
};

DataNode MakeManifest(const Arguments& arguments, const EffectiveSettings& effective,
                      const std::vector<std::string>& commands,
                      const phsp::ReplaySource& source, const phsp::ReplayResults& results,
                      const std::vector<phsp::ReplayHistoryRow>& rows, std::uint64_t tracksOutside)
{
  DataNode manifest = DataNode::MakeObject();
  manifest.Add("application", "phsp-replay");
  manifest.Add("stage", "stage 2, physics only");
  manifest.Add("geant4Version", G4Version);
  manifest.Add("runManager", "MT (G4RunManagerType::MTOnly)");
  manifest.Add("threads", effective.threads);
  manifest.Add("randomEngine", G4Random::getTheEngine()->name());
  manifest.Add("baseSeed", std::to_string(source.BaseSeed()));
  manifest.Add("seeding", "each history reseeded in GeneratePrimaries with HistoryEngineSeeds(baseSeed, fileId, "
                          "historyIndex) (ADR 0015); /random/ commands have no effect on the result");
  manifest.Add("macro", arguments.macro.filename().string());
  manifest.Add("macroCommands", Strings(commands));

  const phsp::HistoryCatalog& catalog = source.Catalog();
  DataNode files = DataNode::MakeArray();
  for (std::size_t i = 0; i < catalog.FileCount(); ++i)
  {
    const phsp::PhaseSpaceReader& file = catalog.File(i);
    const phsp::FileHeader& header = file.Header();
    DataNode node = DataNode::MakeObject();
    node.Add("position", static_cast<long long>(i));
    node.Add("path", file.Path().string());
    node.Add("absolutePath", std::filesystem::absolute(file.Path()).string());
    node.Add("fileId", phsp::FormatId(header.fileId));
    node.Add("contentDigest", phsp::FormatId(header.contentDigest));
    node.Add("formatVersion", static_cast<long long>(header.formatVersion));
    node.Add("stage1Seed", std::to_string(header.stage1Seed));
    node.Add("primariesFired", static_cast<long long>(header.primariesFired));
    node.Add("historiesWritten", static_cast<long long>(header.historiesWritten));
    node.Add("recordsWritten", static_cast<long long>(header.recordsWritten));
    node.Add("firstGlobalIndex", static_cast<long long>(catalog.FirstGlobalIndex(i)));
    node.Add("boxSide_nm", header.boxSideNm);
    node.Add("latticePitch_nm", header.latticePitchNm);
    node.Add("depth_nm", header.depthNm);
    files.Push(node);
  }
  manifest.Add("files", files);
  manifest.Add("historiesInFiles", static_cast<long long>(catalog.HistoryCount()));

  DataNode runs = DataNode::MakeArray();
  for (const phsp::HistoryRange& range : source.Runs())
  {
    DataNode node = DataNode::MakeObject();
    node.Add("first", static_cast<long long>(range.first));
    node.Add("count", static_cast<long long>(range.count));
    runs.Push(node);
  }
  manifest.Add("runs", runs);

  DataNode geometry = DataNode::MakeObject();
  geometry.Add("box", "G4_WATER cube centred at the origin (the stage-1 box frame), region DnaRegion");
  geometry.Add("boxSide_nm", source.BoxSide() / nm);
  geometry.Add("world", "G4_Galactic cube around the box; every track is killed when it leaves the box");
  geometry.Add("worldMargin_nm", phsp::ReplayDetectorConstruction::kWorldMargin / nm);
  manifest.Add("geometry", geometry);

  DataNode physics = DataNode::MakeObject();
  physics.Add("option", phsp::ReplayPhysicsName(arguments.physics));
  physics.Add("description", phsp::ReplayPhysicsDescription(arguments.physics));
  // The EM parameters in which the two physics options differ (README.md,
  // "Stage-2 physics"); stage 1 runs with the stage1 values.
  const G4EmParameters* parameters = G4EmParameters::Instance();
  physics.Add("dnaFast", parameters->DNAFast());
  physics.Add("fluorescence", parameters->Fluo());
  physics.Add("auger", parameters->Auger());
  physics.Add("deexcitationIgnoreCut", parameters->DeexcitationIgnoreCut());
  physics.Add("defaultCut_nm", effective.defaultCut / nm);
  if (const G4Region* region =
          G4RegionStore::GetInstance()->GetRegion(phsp::ReplayDetectorConstruction::kDnaRegionName, false);
      region != nullptr && region->GetProductionCuts() != nullptr)
  {
    const G4ProductionCuts* cuts = region->GetProductionCuts();
    DataNode regionCuts = DataNode::MakeObject();
    for (const char* particle : {"gamma", "e-", "e+", "proton"})
      regionCuts.Add(particle, cuts->GetProductionCut(particle) / nm);
    physics.Add("dnaRegionCuts_nm", regionCuts);
  }
  manifest.Add("physics", physics);

  DataNode scoring = DataNode::MakeObject();
  scoring.Add("primaries", "one event per history: every record as a primary, times shifted so that the "
                           "earliest record of the history is at t = 0");
  scoring.Add("stage2Deposit", "energy deposited in steps whose pre-step point is in the box (the stage-1 "
                               "in-box deposit definition)");
  scoring.Add("historiesReplayed", static_cast<long long>(rows.size()));
  scoring.Add("tracksKilledOutsideBox", static_cast<long long>(tracksOutside));
  scoring.Add("output", "Histories.csv");
  manifest.Add("scoring", scoring);
  manifest.Add("wallTime_s", results.WallTimeS());
  return manifest;
}

int Run(int argc, char** argv)
{
  const Arguments arguments = ParseArguments(argc, argv);

  // Declared before the run manager: the user actions refer to them.
  phsp::ReplaySource source;
  phsp::ReplayResults results;

  std::unique_ptr<G4RunManager> runManager(
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::MTOnly, arguments.threads));
  runManager->SetNumberOfThreads(arguments.threads);

  auto* detector = new phsp::ReplayDetectorConstruction(source);
  runManager->SetUserInitialization(detector);
  auto* physicsList = new phsp::ReplayPhysicsList(arguments.physics);
  runManager->SetUserInitialization(physicsList);
  runManager->SetUserInitialization(new phsp::ReplayActionInitialization(source, results, *detector));

  const std::vector<std::string> commands = ApplyMacro(arguments.macro);
  if (CurrentState() == G4State_PreInit &&
      G4UImanager::GetUIpointer()->ApplyCommand("/run/initialize") != 0)
    throw ExitError(3, "/run/initialize failed");
  if (CurrentState() != G4State_Idle) throw ExitError(3, "Geant4 is not Idle after the macro");
  if (source.Runs().empty())
    throw ExitError(3, "the macro must replay histories (/phsp/source/first k, /run/beamOn N with N >= 1)");

  const std::vector<phsp::ReplayHistoryRow> rows = results.SortedRows();
  std::uint64_t expected = 0;
  for (const auto& range : source.Runs()) expected += range.count;
  if (rows.size() != expected)
    throw ExitError(1, std::to_string(rows.size()) + " histories replayed, " + std::to_string(expected) +
                           " requested (aborted events?)");
  std::uint64_t tracksOutside = 0;
  for (const auto& row : rows) tracksOutside += row.tracksKilledOutsideBox;
  // After the runs: a macro may change the thread count (/run/numberOfThreads)
  // or the default cut (/run/setCut).
  EffectiveSettings effective;
  effective.threads = runManager->GetNumberOfThreads();
  effective.defaultCut = physicsList->GetDefaultCutValue();

  try
  {
    std::filesystem::create_directories(arguments.outputDir);
  }
  catch (const std::exception& e)
  {
    throw ExitError(1, e.what());
  }
  WriteHistoriesCsv(arguments.outputDir / "Histories.csv", rows);
  {
    const auto manifestPath = arguments.outputDir / "Manifest.json";
    std::ofstream out(manifestPath, std::ios::trunc);
    if (!out) throw ExitError(1, "cannot write " + manifestPath.string());
    JsonWriter::Write(out, MakeManifest(arguments, effective, commands, source, results, rows, tracksOutside));
    if (!out) throw ExitError(1, "error writing " + manifestPath.string());
  }

  const auto stage1 = Statistics(rows, [](const phsp::ReplayHistoryRow& r) { return r.stage1DepositEv; });
  const auto stage2 = Statistics(rows, [](const phsp::ReplayHistoryRow& r) { return r.stage2DepositEv; });
  const auto diff =
      Statistics(rows, [](const phsp::ReplayHistoryRow& r) { return r.stage2DepositEv - r.stage1DepositEv; });
  const double z = diff.se > 0 ? diff.mean / diff.se : 0.;
  std::ostringstream done;
  done.precision(6);
  done << "REPLAY_DONE histories=" << rows.size() << " stage1MeanEv=" << stage1.mean << " stage1SeEv=" << stage1.se
       << " stage2MeanEv=" << stage2.mean << " stage2SeEv=" << stage2.se << " diffMeanEv=" << diff.mean
       << " diffSeEv=" << diff.se << " z=" << z << " tracksKilledOutsideBox=" << tracksOutside
       << " physics=" << phsp::ReplayPhysicsName(arguments.physics) << " threads=" << effective.threads
       << " wallTimeS=" << results.WallTimeS() << " output=" << arguments.outputDir.string();
  std::cout << done.str() << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
  // A G4Exception FatalException aborts: no Windows error-report dialog in batch runs.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  try
  {
    return Run(argc, argv);
  }
  catch (const ExitError& e)
  {
    std::cerr << "REPLAY_ERROR " << e.what() << std::endl;
    return e.Status();
  }
  catch (const std::exception& e)
  {
    std::cerr << "REPLAY_ERROR " << e.what() << std::endl;
    return 1;
  }
}
