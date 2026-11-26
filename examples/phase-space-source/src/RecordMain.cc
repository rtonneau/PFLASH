// phsp-record: stage 1 of the phase-space source. Fires the beam of a macro
// (G4GeneralParticleSource, /gps/...) into a water phantom and writes every
// Inward crossing of a scoring box into one Phase-space file (ADR 0014).
//
//   phsp-record <macro> --output <dir>/<name> [--threads N] [--seed S]
//
//   <macro>     Geant4 commands applied line by line (blank and '#' lines are
//               skipped; a failing command is an error): /phsp/geom/... before
//               /run/initialize, then /gps/... and exactly one /run/beamOn N.
//               /run/initialize is issued after the macro if it did not.
//               /random/... commands are refused: the seed is --seed, and it is
//               written in the file header.
//   --output    writes <dir>/<name>.phsp and <dir>/<name>.json (a trailing
//               ".phsp" is accepted); <dir> is created if missing; existing
//               files of that name are replaced
//   --threads   worker threads, 1..256 (default 1). Always the MT run manager:
//               the master draws every event's seeds in event order, so the file
//               does not depend on N
//   --seed      stage-1 seed, an unsigned 64-bit integer (default 1)
//
// One history per (primary, box entered), written in (stage-1 event ID, box iy,
// box ix) order; a primary with no record is counted in the header only.
// Output on success: the two files and a line
//   RECORD_DONE primaries=.. primariesWithRecords=.. histories=.. records=.. ...
// hitFraction = primariesWithRecords / primaries.
// Exit status: 0 success, 2 bad arguments, 3 macro or initialization failure,
// 1 failure during the run or while writing. A G4Exception (FatalException)
// aborts the process.
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
#include "PhaseSpaceFormat.hh"
#include "PhaseSpaceWriter.hh"
#include "RecordActions.hh"
#include "RecordDetectorConstruction.hh"
#include "RecordPhysicsList.hh"

#include "G4Electron.hh"
#include "G4NistManager.hh"
#include "G4ProductionCuts.hh"
#include "G4ProductionCutsTable.hh"
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
constexpr const char* kUsage = "usage: phsp-record <macro> --output <dir>/<name> [--threads N] [--seed S]";

struct Arguments
{
  std::filesystem::path macro;
  std::filesystem::path phspPath;
  int threads = 1;
  std::uint64_t seed = 1;
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
  bool haveMacro = false, haveOutput = false, haveThreads = false, haveSeed = false;
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
      arguments.phspPath = value;
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
    else if (option == "--seed" && !haveSeed)
    {
      arguments.seed = ParseUnsigned(option, value);
      haveSeed = true;
    }
    else
      throw ExitError(2, "unknown or repeated option " + option);
  }
  if (!haveMacro || !haveOutput) throw ExitError(2, kUsage);
  if (!std::filesystem::is_regular_file(arguments.macro))
    throw ExitError(2, "macro not found: " + arguments.macro.string());
  if (arguments.phspPath.filename().empty() || arguments.phspPath.filename() == "." ||
      arguments.phspPath.filename() == "..")
    throw ExitError(2, "--output needs <dir>/<name>, got '" + arguments.phspPath.string() + "'");
  if (arguments.phspPath.extension() != ".phsp") arguments.phspPath += ".phsp";
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

void CheckGeometry(const phsp::RecordDetectorConstruction& detector, const std::string& where)
{
  if (const auto problem = detector.Problem(); !problem.empty())
    throw ExitError(3, where + ": inconsistent geometry: " + problem);
}

// Applies the macro line by line; returns the commands applied.
std::vector<std::string> ApplyMacro(const std::filesystem::path& macro, const phsp::RecordDetectorConstruction& detector)
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
    if (command.rfind("/random/", 0) == 0)
      throw ExitError(3, where + ": /random/ commands are not allowed (the stage-1 seed is --seed): " + command);
    if (command.rfind("/run/initialize", 0) == 0 && CurrentState() == G4State_PreInit) CheckGeometry(detector, where);
    if (const int status = ui->ApplyCommand(command); status != 0)
      throw ExitError(3, where + ": command failed (G4UImanager code " + std::to_string(status) + "): " + command);
    applied.push_back(command);
  }
  if (in.bad()) throw ExitError(3, "error reading macro " + macro.string());
  return applied;
}

// Master-engine seeds from the stage-1 seed: two positive 31-bit values.
// Kept alive for the whole process (MixMaxRng keeps the pointer).
long gEngineSeeds[3] = {1, 1, 0};

void SeedMasterEngine(std::uint64_t seed)
{
  const std::uint64_t mixed = phsp::SplitMix64(seed);
  gEngineSeeds[0] = static_cast<long>((mixed >> 33) & 0x7fffffffULL);
  gEngineSeeds[1] = static_cast<long>(mixed & 0x7fffffffULL);
  for (int i = 0; i < 2; ++i)
    if (gEngineSeeds[i] == 0) gEngineSeeds[i] = 1;
  gEngineSeeds[2] = 0;
  G4Random::setTheSeeds(gEngineSeeds);
}

DataNode Strings(const std::vector<std::string>& values)
{
  DataNode array = DataNode::MakeArray();
  for (const auto& value : values) array.Push(DataNode(value));
  return array;
}

// Everything here must be the same for any thread count (the sidecar is
// compared byte by byte between thread counts): no thread count, no timing.
DataNode MakeStage1Info(const Arguments& arguments, const std::vector<std::string>& commands,
                        const phsp::RecordDetectorConstruction& detector, const G4VUserPhysicsList& physics,
                        const phsp::RecordOutcome& outcome)
{
  const phsp::RecordGeometry& geom = detector.Geometry();
  DataNode info = DataNode::MakeObject();
  info.Add("application", "phsp-record");
  info.Add("geant4Version", G4Version);
  info.Add("runManager", "MT (G4RunManagerType::MTOnly); event seeds drawn by the master in event order");
  info.Add("randomEngine", G4Random::getTheEngine()->name());
  DataNode engineSeeds = DataNode::MakeArray();
  engineSeeds.Push(static_cast<long long>(gEngineSeeds[0])).Push(static_cast<long long>(gEngineSeeds[1]));
  info.Add("masterEngineSeeds", engineSeeds);
  info.Add("macro", arguments.macro.filename().string());
  info.Add("macroCommands", Strings(commands));

  DataNode geometry = DataNode::MakeObject();
  geometry.Add("world", "G4_Galactic cube around the phantom");
  geometry.Add("worldMargin_nm", phsp::RecordDetectorConstruction::kWorldMargin / nm);
  geometry.Add("phantom", "G4_WATER cube centred at the origin; the beam enters through its -z face");
  geometry.Add("phantomSide_nm", geom.phantomSide / nm);
  geometry.Add("entranceFaceZ_nm", -0.5 * geom.phantomSide / nm);
  geometry.Add("depth_nm", geom.depth / nm);
  geometry.Add("boxCentreZ_nm", detector.BoxCentreZ() / nm);
  geometry.Add("boxSide_nm", geom.boxSide / nm);
  geometry.Add("shell_nm", geom.shell / nm);
  geometry.Add("latticePitch_nm", geom.latticePitch / nm);
  geometry.Add("boxesPerSide", 2 * detector.HalfCount() + 1);
  geometry.Add("boxCount", detector.BoxCount());
  geometry.Add("dnaShell", "G4_WATER cube of side boxSide + 2 x shell around each box (region DnaRegion), "
                           "a daughter of the phantom; the box is its only daughter");
  geometry.Add("dnaShellSide_nm", detector.ShellSide() / nm);
  info.Add("geometry", geometry);

  DataNode physicsNode = DataNode::MakeObject();
  physicsNode.Add("outsideDnaRegion", "G4EmStandardPhysics_option4");
  physicsNode.Add("dnaRegion", phsp::RecordDetectorConstruction::kDnaRegionName);
  physicsNode.Add("dnaRegionModels", std::string(phsp::RecordPhysicsList::kDnaOption) +
                                         " via G4EmParameters::AddDNA + G4EmDNAPhysicsActivator (e- DNA models up to 1 MeV)");
  const G4double defaultCut = physics.GetDefaultCutValue();
  physicsNode.Add("defaultCut_nm", defaultCut / nm);
  const G4Material* water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
  physicsNode.Add("defaultElectronCutInWater_eV",
                  G4ProductionCutsTable::GetProductionCutsTable()->ConvertRangeToEnergy(G4Electron::Electron(), water,
                                                                                         defaultCut) /
                      eV);
  if (const G4Region* region =
          G4RegionStore::GetInstance()->GetRegion(phsp::RecordDetectorConstruction::kDnaRegionName, false);
      region != nullptr && region->GetProductionCuts() != nullptr)
  {
    const G4ProductionCuts* cuts = region->GetProductionCuts();
    DataNode regionCuts = DataNode::MakeObject();
    for (const char* particle : {"gamma", "e-", "e+", "proton"})
      regionCuts.Add(particle, cuts->GetProductionCut(particle) / nm);
    physicsNode.Add("dnaRegionCuts_nm", regionCuts);
  }
  info.Add("physics", physicsNode);

  DataNode recording = DataNode::MakeObject();
  recording.Add("rule", "every inward crossing of any track into a scoring box (pre-step point with status "
                        "fGeomBoundary in a box); nothing is killed (ADR 0014)");
  recording.Add("history", "one history per (primary, box entered), in (stage-1 event ID, box iy, box ix) order");
  recording.Add("frame", "origin at the centre of the box entered (touched box's translation), phantom axes");
  recording.Add("inBoxDeposit", "energy deposited in steps whose pre-step point is in a scoring box");
  recording.Add("eventBoxPairsWithDepositButNoRecord", static_cast<long long>(outcome.depositWithoutRecordEvents));
  recording.Add("depositWithoutRecord_eV", outcome.depositWithoutRecordEv);
  info.Add("recording", recording);
  return info;
}

int Run(int argc, char** argv)
{
  const Arguments arguments = ParseArguments(argc, argv);

  phsp::RecordOutcome outcome;
  std::unique_ptr<G4RunManager> runManager(
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::MTOnly, arguments.threads));
  runManager->SetNumberOfThreads(arguments.threads);
  SeedMasterEngine(arguments.seed);

  auto* detector = new phsp::RecordDetectorConstruction();
  auto* physics = new phsp::RecordPhysicsList();
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(physics);
  runManager->SetUserInitialization(new phsp::RecordActionInitialization(*detector, outcome));

  const std::vector<std::string> commands = ApplyMacro(arguments.macro, *detector);
  if (CurrentState() == G4State_PreInit)
  {
    CheckGeometry(*detector, "before /run/initialize");
    if (G4UImanager::GetUIpointer()->ApplyCommand("/run/initialize") != 0) throw ExitError(3, "/run/initialize failed");
  }
  if (CurrentState() != G4State_Idle) throw ExitError(3, "Geant4 is not Idle after the macro");
  if (outcome.runsWithEvents != 1 || outcome.primariesFired == 0)
    throw ExitError(3, "the macro must start exactly one run with events (/run/beamOn N, N >= 1)");

  const phsp::RecordGeometry& geom = detector->Geometry();
  phsp::FileDescription description;
  description.stage1Seed = arguments.seed;
  description.primariesFired = outcome.primariesFired;
  description.latticePitchNm = geom.latticePitch / nm;
  description.boxSideNm = geom.boxSide / nm;
  description.depthNm = geom.depth / nm;

  if (const auto directory = arguments.phspPath.parent_path(); !directory.empty())
    std::filesystem::create_directories(directory);
  const DataNode info = MakeStage1Info(arguments, commands, *detector, *physics, outcome);
  phsp::WrittenFile written;
  try
  {
    written = phsp::WritePhaseSpaceFile(arguments.phspPath, description, outcome.histories, info);
  }
  catch (const std::exception& e)
  {
    throw ExitError(1, e.what());
  }

  const auto primaries = static_cast<double>(outcome.primariesFired);
  const auto histories = static_cast<double>(written.header.historiesWritten);
  const auto records = static_cast<double>(written.header.recordsWritten);
  // Histories are per (primary, box) and sorted by event ID first.
  std::uint64_t primariesWithRecords = 0;
  for (std::size_t i = 0; i < outcome.histories.size(); ++i)
    if (i == 0 || outcome.histories[i].stage1EventId != outcome.histories[i - 1].stage1EventId) ++primariesWithRecords;
  std::ostringstream done;
  done.precision(6);
  done << "RECORD_DONE primaries=" << outcome.primariesFired << " primariesWithRecords=" << primariesWithRecords
       << " histories=" << written.header.historiesWritten << " records=" << written.header.recordsWritten
       << " meanRecordsPerHistory=" << (histories > 0 ? records / histories : 0.)
       << " hitFraction=" << static_cast<double>(primariesWithRecords) / primaries
       << " historiesPerHitPrimary="
       << (primariesWithRecords > 0 ? histories / static_cast<double>(primariesWithRecords) : 0.)
       << " meanInBoxDepositPerHistoryEv=" << (histories > 0 ? written.totalInBoxDepositEv / histories : 0.)
       << " meanInBoxDepositPerPrimaryEv=" << written.totalInBoxDepositEv / primaries
       << " threads=" << outcome.threads << " wallTimeS=" << outcome.wallTimeS
       << " timePerPrimaryMs=" << 1e3 * outcome.wallTimeS * outcome.threads / primaries
       << " fileId=" << phsp::FormatId(written.header.fileId) << " file=" << written.phspPath.string();
  if (outcome.depositWithoutRecordEvents > 0)
    std::cout << "RECORD_WARNING " << outcome.depositWithoutRecordEvents
              << " (event, box) pairs deposited energy in a box without an inward crossing (" << outcome.depositWithoutRecordEv
              << " eV, not written): did the beam start inside a box?" << std::endl;
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
    std::cerr << "RECORD_ERROR " << e.what() << std::endl;
    return e.Status();
  }
  catch (const std::exception& e)
  {
    std::cerr << "RECORD_ERROR " << e.what() << std::endl;
    return 1;
  }
}
