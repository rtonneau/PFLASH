// capture: shoot N primaries along +z through a water world, each crossing
// the embedded capture box at a random (x, y) over its face, and write, for
// each, the precursor molecules inside the box when the chemistry starts
// (1 ps) and the energy deposited inside it as a PrecursorPacket, plus the
// library index and a JSON summary (see CaptureRecorder.hh).
//
//   capture --macro <file> --output <dir> --count <N> --first-seed <S>
//
//   --macro       Geant4 commands applied line by line before the run (gun
//                 particle and energy, /drs/capture/boxHalfSide, ...); the gun
//                 position and direction are overridden; blank and '#' lines are skipped,
//                 a failing command or /run/beamOn is an error. /run/initialize
//                 is issued after it if the macro did not.
//   --output      library directory; created if missing, must be empty
//   --count       number of primaries = packets, 1..1000000
//   --first-seed  physics seed of event 0; event i uses S + i (1 <= S + i <= 2147483647)
//   --max-empty   events without any precursor molecule in the box that may be
//                 skipped (default 0: such an event is an error); a skipped event
//                 gives no packet, so packet seeds are then no longer S + index;
//                 skipped seeds and deposits are listed in capture.json
//
// Output: <dir>/packet-00000.txt ..., <dir>/library.csv, <dir>/capture.json
// (see CaptureLibrary.hh). Exit status 0 and a "CAPTURE_DONE" line on success;
// otherwise a "CAPTURE_ERROR <reason>" line on stderr and a nonzero status:
// 2 bad arguments, 3 macro or initialization failure, 4 chemistry model is not
// IRT_syn, 1 failure during the run or while writing the library.
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CaptureActions.hh"
#include "CaptureDetectorConstruction.hh"
#include "CaptureLibrary.hh"
#include "CaptureRecorder.hh"
#include "DrsPhysicsList.hh"
#include "DrsPhysicsTag.hh"
#include "Ledger.hh"
#include "PrecursorPacket.hh"

#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4Version.hh"
#include "Randomize.hh"

namespace
{

constexpr long long kMaxSeed = std::numeric_limits<int>::max();  // G4Random::setTheSeed takes a 32-bit long on Windows
constexpr long long kMaxCount = 1'000'000;

struct Arguments
{
  std::filesystem::path macro;
  std::filesystem::path output;
  long long count = 0;
  long long firstSeed = 0;
  long long maxEmpty = 0;
};

class ExitError : public std::runtime_error
{
 public:
  ExitError(int status, const std::string& message) : std::runtime_error(message), fStatus(status) {}
  int Status() const { return fStatus; }

 private:
  int fStatus;
};

long long ParseInteger(const std::string& option, const std::string& text)
{
  std::size_t used = 0;
  long long value = 0;
  try
  {
    value = std::stoll(text, &used, 10);
  }
  catch (const std::exception&)
  {
    used = 0;
  }
  if (used == 0 || used != text.size())
    throw ExitError(2, option + " needs an integer, got '" + text + "'");
  return value;
}

Arguments ParseArguments(int argc, char** argv)
{
  Arguments arguments;
  bool haveMacro = false, haveOutput = false, haveCount = false, haveSeed = false, haveMaxEmpty = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (i + 1 >= argc) throw ExitError(2, "missing value after " + option);
    const std::string value = argv[++i];
    if (option == "--macro" && !haveMacro)
    {
      arguments.macro = value;
      haveMacro = true;
    }
    else if (option == "--output" && !haveOutput)
    {
      arguments.output = value;
      haveOutput = true;
    }
    else if (option == "--count" && !haveCount)
    {
      arguments.count = ParseInteger(option, value);
      haveCount = true;
    }
    else if (option == "--max-empty" && !haveMaxEmpty)
    {
      arguments.maxEmpty = ParseInteger(option, value);
      haveMaxEmpty = true;
    }
    else if (option == "--first-seed" && !haveSeed)
    {
      arguments.firstSeed = ParseInteger(option, value);
      haveSeed = true;
    }
    else
      throw ExitError(2, "unknown or repeated option " + option);
  }
  if (!haveMacro || !haveOutput || !haveCount || !haveSeed)
    throw ExitError(2, "usage: capture --macro <file> --output <dir> --count <N> --first-seed <S>");
  if (arguments.count < 1 || arguments.count > kMaxCount)
    throw ExitError(2, "--count must be in 1.." + std::to_string(kMaxCount));
  if (arguments.maxEmpty < 0 || arguments.maxEmpty > kMaxCount)
    throw ExitError(2, "--max-empty must be in 0.." + std::to_string(kMaxCount));
  if (arguments.firstSeed < 1 || arguments.firstSeed > kMaxSeed - (arguments.count + arguments.maxEmpty - 1))
    throw ExitError(2, "--first-seed S needs 1 <= S and S + count + max-empty - 1 <= " + std::to_string(kMaxSeed));
  if (!std::filesystem::is_regular_file(arguments.macro))
    throw ExitError(2, "macro not found: " + arguments.macro.string());
  arguments.macro = std::filesystem::absolute(arguments.macro);
  return arguments;
}

void PrepareOutputDirectory(const std::filesystem::path& directory)
{
  if (std::filesystem::exists(directory))
  {
    if (!std::filesystem::is_directory(directory))
      throw ExitError(2, "--output is not a directory: " + directory.string());
    if (!std::filesystem::is_empty(directory))
      throw ExitError(2, "--output must be empty (refusing to mix libraries): " + directory.string());
  }
  else
    std::filesystem::create_directories(directory);
}

std::string Trim(const std::string& text)
{
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

void ApplyMacro(const std::filesystem::path& macro)
{
  std::ifstream in(macro);
  if (!in) throw ExitError(3, "cannot open macro " + macro.string());
  auto* ui = G4UImanager::GetUIpointer();
  std::string line;
  int lineNumber = 0;
  while (std::getline(in, line))
  {
    ++lineNumber;
    const auto command = Trim(line);
    if (command.empty() || command.front() == '#') continue;
    const auto where = macro.filename().string() + ":" + std::to_string(lineNumber);
    if (command.rfind("/run/beamOn", 0) == 0)
      throw ExitError(3, where + ": the macro must not start the run (capture calls BeamOn)");
    if (const int status = ui->ApplyCommand(command); status != 0)
      throw ExitError(3, where + ": command failed (G4UImanager code " + std::to_string(status) +
                             "): " + command);
  }
  if (in.bad()) throw ExitError(3, "error reading macro " + macro.string());
}

G4ApplicationState CurrentState()
{
  return G4StateManager::GetStateManager()->GetCurrentState();
}

drs::JsonValue Vector3(const std::array<double, 3>& v)
{
  auto array = drs::JsonValue::Array();
  for (double x : v) array.Append(drs::JsonValue::Real(x));
  return array;
}

void WriteSummary(const std::filesystem::path& path, const Arguments& arguments,
                  G4double halfSide, G4double worldHalfSide, const drs::CaptureRecorder& recorder,
                  const drs::PacketLimits& limits)
{
  using drs::JsonValue;
  const auto& entries = recorder.Entries();
  const auto totals = drs::SumLibrary(entries);

  auto root = JsonValue::Object();
  root.Set("format", JsonValue::String("drs-capture-summary"));
  root.Set("formatVersion", JsonValue::Integer(1));
  root.Set("compatibilityTag", JsonValue::String(drs::kPacketCompatibilityTag));
  root.Set("geant4Version", JsonValue::String(G4Version));
  root.Set("physics", JsonValue::String("G4EmDNAPhysics_option2"));
  root.Set("chemistry", JsonValue::String("PureWater (copied network + acid-base buffer), pH 7, IRT_syn"));
  root.Set("chemistryTimeStepModel", JsonValue::String("IRT_syn"));
  root.Set("medium", JsonValue::String("G4_WATER, pH 7 acid-base buffer (H3Op(B), OHm(B)), no O2"));
  root.Set("randomEngine", JsonValue::String(G4Random::getTheEngine()->name()));
  root.Set("macro", JsonValue::String(arguments.macro.string()));
  root.Set("captureHalfSide_nm", JsonValue::Real(halfSide / nm));
  root.Set("worldHalfSide_nm", JsonValue::Real(worldHalfSide / nm));
  root.Set("beam", JsonValue::String("+z, from 1 nm inside the -z world face, (x, y) uniform over the box face"));
  root.Set("captureTime_ns", JsonValue::Real(picosecond / ns));
  root.Set("firstPhysicsSeed", JsonValue::Integer(arguments.firstSeed));
  root.Set("maxEmptyEvents", JsonValue::Integer(arguments.maxEmpty));
  auto skipped = JsonValue::Array();
  for (const auto& skippedEvent : recorder.Skipped())
  {
    auto item = JsonValue::Object();
    item.Set("physicsSeed", JsonValue::Integer(skippedEvent.physicsSeed));
    item.Set("energyDeposit_eV", JsonValue::Real(skippedEvent.energyDepositEv));
    skipped.Append(item);
  }
  root.Set("skippedEmptyEvents", skipped);
  root.Set("libraryIndex", JsonValue::String(drs::kLibraryIndexName));

  auto tolerance = JsonValue::Object();
  tolerance.Set("energyAbsTol_eV", JsonValue::Real(limits.energyAbsTolEv));
  tolerance.Set("energyRelTol", JsonValue::Real(limits.energyRelTol));
  root.Set("energyBalanceTolerance", tolerance);

  auto total = JsonValue::Object();
  total.Set("packetCount", JsonValue::Integer(static_cast<long long>(totals.packetCount)));
  total.Set("moleculeCount", JsonValue::Integer(static_cast<long long>(totals.moleculeCount)));
  total.Set("primaryEnergy_eV", JsonValue::Real(totals.primaryEnergyEv));
  total.Set("energyDeposit_eV", JsonValue::Real(totals.energyDepositEv));
  total.Set("escapedEnergy_eV", JsonValue::Real(totals.escapedEnergyEv));
  total.Set("maxAbsEnergyResidual_eV", JsonValue::Real(totals.maxAbsResidualEv));
  // Mean energy deposited in the box per primary (the dose of one packet).
  root.Set("meanBoxDeposit_eV",
           JsonValue::Real(totals.packetCount > 0 ? totals.energyDepositEv / static_cast<double>(totals.packetCount) : 0.0));
  double outsideDeposit = 0;
  double worldEscape = 0;
  long long dropped = 0;
  for (const auto& p : recorder.Provenance())
  {
    outsideDeposit += p.outsideDepositEv;
    worldEscape += p.worldEscapeEv;
    dropped += static_cast<long long>(p.droppedMolecules);
  }
  total.Set("outsideDeposit_eV", JsonValue::Real(outsideDeposit));
  total.Set("worldEscape_eV", JsonValue::Real(worldEscape));
  total.Set("droppedMoleculeCount", JsonValue::Integer(dropped));
  root.Set("totals", total);

  auto packets = JsonValue::Array();
  const auto& provenance = recorder.Provenance();
  for (std::size_t i = 0; i < entries.size(); ++i)
  {
    const auto& e = entries[i];
    auto packet = JsonValue::Object();
    packet.Set("packetIndex", JsonValue::Integer(static_cast<long long>(e.packetIndex)));
    packet.Set("file", JsonValue::String(e.file));
    packet.Set("physicsSeed", JsonValue::Integer(e.physicsSeed));
    packet.Set("primaryParticle", JsonValue::String(e.primaryParticle));
    packet.Set("primaryOrigin_nm", Vector3(provenance[i].primaryOriginNm));
    packet.Set("primaryDirection", Vector3(provenance[i].primaryDirection));
    packet.Set("primaryEnergy_eV", JsonValue::Real(e.primaryEnergyEv));
    packet.Set("energyDeposit_eV", JsonValue::Real(e.energyDepositEv));
    packet.Set("escapedEnergy_eV", JsonValue::Real(e.escapedEnergyEv));
    packet.Set("energyResidual_eV", JsonValue::Real(e.energyResidualEv));
    packet.Set("moleculeCount", JsonValue::Integer(static_cast<long long>(e.moleculeCount)));
    packet.Set("outsideDeposit_eV", JsonValue::Real(provenance[i].outsideDepositEv));
    packet.Set("worldEscape_eV", JsonValue::Real(provenance[i].worldEscapeEv));
    packet.Set("droppedMoleculeCount", JsonValue::Integer(static_cast<long long>(provenance[i].droppedMolecules)));
    packets.Append(packet);
  }
  root.Set("packets", packets);

  std::ofstream out(path, std::ios::binary);
  if (!out) throw ExitError(1, "cannot open " + path.string());
  drs::WriteJson(out, root);
  out << '\n';
  out.close();
  if (!out) throw ExitError(1, "cannot write " + path.string());
}

int Run(int argc, char** argv)
{
  const auto arguments = ParseArguments(argc, argv);
  PrepareOutputDirectory(arguments.output);

  const drs::PacketLimits limits;
  drs::CaptureSettings settings;
  settings.outputDirectory = arguments.output;
  settings.firstPhysicsSeed = arguments.firstSeed;
  settings.maxEmpty = static_cast<std::size_t>(arguments.maxEmpty);
  settings.limits = limits;
  drs::CaptureRecorder recorder(settings);
  // Declared before the run manager so it outlives it: G4Scheduler keeps a
  // pointer to it and never deletes it.
  drs::CaptureTimeStepAction timeStepAction(recorder);

  std::unique_ptr<G4RunManager> runManager(
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::SerialOnly));
  auto* detector = new drs::CaptureDetectorConstruction();
  runManager->SetUserInitialization(detector);
  runManager->SetUserInitialization(new drs::DrsPhysicsList());
  runManager->SetUserInitialization(new drs::CaptureActionInitialization(recorder, &timeStepAction));

  ApplyMacro(arguments.macro);
  if (CurrentState() == G4State_PreInit &&
      G4UImanager::GetUIpointer()->ApplyCommand("/run/initialize") != 0)
    throw ExitError(3, "/run/initialize failed");
  if (CurrentState() != G4State_Idle) throw ExitError(3, "Geant4 is not Idle after initialization");

  if (const auto problem = drs::DescribeIrtSynParameterProblem(); !problem.empty())
    throw ExitError(4, problem);

  recorder.SetGeometry(detector->GetBoxHalfSide(), detector->GetWorldHalfSide(), detector->GetBoxVolume());
  runManager->BeamOn(static_cast<G4int>(arguments.count));
  // Skipped empty events left packets missing: one more run per round, seeds continuing.
  while (recorder.Failure().empty() && recorder.Entries().size() < static_cast<std::size_t>(arguments.count))
    runManager->BeamOn(static_cast<G4int>(arguments.count - static_cast<long long>(recorder.Entries().size())));

  if (!recorder.Failure().empty()) throw ExitError(1, recorder.Failure());
  const auto& entries = recorder.Entries();
  if (entries.size() != static_cast<std::size_t>(arguments.count))
    throw ExitError(1, "captured " + std::to_string(entries.size()) + " packets, expected " +
                           std::to_string(arguments.count));

  const auto indexPath = arguments.output / drs::kLibraryIndexName;
  {
    std::ofstream out(indexPath, std::ios::binary);
    if (!out) throw ExitError(1, "cannot open " + indexPath.string());
    drs::WriteLibraryIndex(out, entries);
    out.close();
    if (!out) throw ExitError(1, "cannot write " + indexPath.string());
  }
  // Read the whole library back the way consumers will.
  const auto library = drs::LoadPacketLibrary(arguments.output, drs::kPacketCompatibilityTag, limits);
  if (library.entries != entries) throw ExitError(1, "library.csv does not read back to the captured entries");

  WriteSummary(arguments.output / "capture.json", arguments, detector->GetBoxHalfSide(),
               detector->GetWorldHalfSide(), recorder, limits);

  const auto totals = drs::SumLibrary(entries);
  std::ostringstream done;
  done.precision(17);
  done << "CAPTURE_DONE packets=" << totals.packetCount << " molecules=" << totals.moleculeCount
       << " primaryEnergyEv=" << totals.primaryEnergyEv << " energyDepositEv=" << totals.energyDepositEv
       << " escapedEnergyEv=" << totals.escapedEnergyEv
       << " meanBoxDepositEv=" << totals.energyDepositEv / static_cast<double>(totals.packetCount)
       << " maxAbsResidualEv=" << totals.maxAbsResidualEv << " output=" << arguments.output.string();
  std::cout << done.str() << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  try
  {
    return Run(argc, argv);
  }
  catch (const ExitError& error)
  {
    std::cerr << "CAPTURE_ERROR " << error.what() << std::endl;
    return error.Status();
  }
  catch (const std::exception& error)
  {
    std::cerr << "CAPTURE_ERROR " << error.what() << std::endl;
    return 1;
  }
}
