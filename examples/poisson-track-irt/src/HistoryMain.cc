// history: one shared IRT_syn chemistry history fed by precursor packets.
//
//   history --config <file> --output <dir>
//
// Approach. The primaries were transported separately by `capture`, and the
// precursors of each track were captured at 1 ps (ticket 02). Here the packets
// of an arrival schedule are injected into ONE shared IRT_syn chemistry run:
// each packet ("source") is rigidly translated to its offset and pushed into
// the delayed lists at arrival time + capture age (SourceInjector), then the
// chemistry runs standalone (G4DNAChemistryManager::UseAsStandalone(true), then
// Run(); no event loop, no physics transport). Each source therefore activates
// at its own time, inside the history left by the earlier sources, and reacts
// with all of them. ActivationObserver checks every activation and the
// preservation of the earlier molecules, and reads the species counts.
//
// Modes (config file, see HistoryConfig.hh):
//   deterministic  every row of a schedule file;
//   single         one row of the schedule file, alone (the control history of
//                  that source), on the record grid of the full schedule;
//   poisson        a Poisson arrival process drawn from sourceSeed.
// The chemistry seed (chemistrySeed) is separate from the source seed.
//
// Output in <dir> (created if missing, must be empty):
//   species.csv      molecule counts per species on the record grid, absolute
//                    time (timeNs,species,count,history,chemistrySeed);
//   activations.csv  one row per source: scheduled, expected and observed
//                    activation times, track IDs, energies;
//   ledger.json      seeds, settings, library and source energies, the checks.
// Exit status 0 and a "HISTORY_DONE" line on success; otherwise a
// "HISTORY_ERROR <reason>" line on stderr and a nonzero status: 2 bad
// arguments, config, schedule or packets; 3 initialization failure; 4 the
// chemistry model is not IRT_syn; 1 a failed check, or a failure during the
// history or while writing the outputs.
#include <chrono>
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

#include "ActivationObserver.hh"
#include "ArrivalSchedule.hh"
#include "CaptureLibrary.hh"
#include "ExamplePhysicsList.hh"
#include "ExamplePhysicsTag.hh"
#include "HistoryConfig.hh"
#include "HistoryDetectorConstruction.hh"
#include "Ledger.hh"
#include "SourceInjector.hh"

#include "G4DNAChemistryManager.hh"
#include "G4H2O.hh"
#include "G4MoleculeCounter.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeCounterTimeComparer.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4Scheduler.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Version.hh"
#include "Randomize.hh"

namespace
{

struct Arguments
{
  std::filesystem::path config;
  std::filesystem::path output;
};

class ExitError : public std::runtime_error
{
 public:
  ExitError(int status, const std::string& message) : std::runtime_error(message), fStatus(status) {}
  int Status() const { return fStatus; }

 private:
  int fStatus;
};

Arguments ParseArguments(int argc, char** argv)
{
  Arguments arguments;
  bool haveConfig = false, haveOutput = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string option = argv[i];
    if (i + 1 >= argc) throw ExitError(2, "missing value after " + option);
    const std::string value = argv[++i];
    if (option == "--config" && !haveConfig)
    {
      arguments.config = value;
      haveConfig = true;
    }
    else if (option == "--output" && !haveOutput)
    {
      arguments.output = value;
      haveOutput = true;
    }
    else
      throw ExitError(2, "unknown or repeated option " + option);
  }
  if (!haveConfig || !haveOutput) throw ExitError(2, "usage: history --config <file> --output <dir>");
  if (!std::filesystem::is_regular_file(arguments.config))
    throw ExitError(2, "config not found: " + arguments.config.string());
  arguments.config = std::filesystem::absolute(arguments.config);
  return arguments;
}

void PrepareOutputDirectory(const std::filesystem::path& directory)
{
  if (std::filesystem::exists(directory))
  {
    if (!std::filesystem::is_directory(directory))
      throw ExitError(2, "--output is not a directory: " + directory.string());
    if (!std::filesystem::is_empty(directory))
      throw ExitError(2, "--output must be empty (refusing to mix histories): " + directory.string());
  }
  else
    std::filesystem::create_directories(directory);
}

// The arrivals to inject, and the arrival times the record grid is built on
// (the full schedule in single mode).
struct Schedule
{
  std::vector<ptirt::Arrival> arrivals;
  std::vector<double> gridArrivalTimesNs;
};

std::vector<ptirt::Arrival> ReadScheduleFile(const std::filesystem::path& file, const ptirt::ScheduleBounds& bounds)
{
  std::ifstream in(file);
  if (!in) throw ExitError(2, "cannot open schedule " + file.string());
  try
  {
    return ptirt::ParseDeterministicSchedule(in, bounds);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "schedule " + file.string() + ": " + error.what());
  }
}

Schedule MakeSchedule(const ptirt::HistoryConfig& config, std::size_t libraryPacketCount)
{
  ptirt::ScheduleBounds bounds;
  bounds.libraryPacketCount = libraryPacketCount;
  bounds.endpointNs = config.endpointNs;

  Schedule schedule;
  switch (config.mode)
  {
    case ptirt::HistoryMode::Deterministic:
      schedule.arrivals = ReadScheduleFile(config.scheduleFile, bounds);
      break;
    case ptirt::HistoryMode::Single:
    {
      const auto full = ReadScheduleFile(config.scheduleFile, bounds);
      if (config.singleSource >= full.size())
        throw ExitError(2, "singleSource " + std::to_string(config.singleSource) + " is not a row of the " +
                               std::to_string(full.size()) + "-row schedule " + config.scheduleFile.string());
      for (const auto& arrival : full) schedule.gridArrivalTimesNs.push_back(arrival.timeNs);
      schedule.arrivals = {full[config.singleSource]};
      return schedule;
    }
    case ptirt::HistoryMode::Poisson:
      try
      {
        schedule.arrivals = ptirt::SamplePoissonArrivals(config.poisson, bounds);
      }
      catch (const std::runtime_error& error)
      {
        throw ExitError(2, std::string("poisson schedule: ") + error.what());
      }
      break;
  }
  for (const auto& arrival : schedule.arrivals) schedule.gridArrivalTimesNs.push_back(arrival.timeNs);
  return schedule;
}

// Registered before /run/initialize, like dnachem-min: G4DNAChemistryManager
// initializes the counters it finds. One counter per species configuration,
// H2O ignored, times within 1 ps merged (the record grid keeps kGridGuardNs
// away from the arrivals). Reset before each event: there is no event here, so
// the counter only holds this history.
G4int RegisterCounter()
{
  auto* manager = G4MoleculeCounterManager::Instance();
  manager->SetResetCountersBeforeEvent(true);
  auto counter = std::make_unique<G4MoleculeCounter>("BasicCounter");
  counter->IgnoreMolecule(G4H2O::Definition());
  counter->SetTimeComparer(G4MoleculeCounterTimeComparer::CreateWithFixedPrecision(1 * ps));
  return manager->RegisterCounter(std::move(counter));
}

template <typename Writer>
void WriteFile(const std::filesystem::path& path, Writer write)
{
  std::ofstream out(path, std::ios::binary);
  if (!out) throw ExitError(1, "cannot open " + path.string());
  write(out);
  out.close();
  if (!out) throw ExitError(1, "cannot write " + path.string());
}

ptirt::JsonValue Vector3(const std::array<double, 3>& v)
{
  auto array = ptirt::JsonValue::Array();
  for (double x : v) array.Append(ptirt::JsonValue::Real(x));
  return array;
}

ptirt::JsonValue Count(std::size_t n)
{
  return ptirt::JsonValue::Integer(static_cast<long long>(n));
}

struct LedgerInput
{
  const Arguments& arguments;
  const ptirt::HistoryConfig& config;
  const ptirt::PacketLibrary& library;
  const Schedule& schedule;
  const std::vector<double>& grid;
  const std::vector<ptirt::ActivationRecord>& records;
  const ptirt::ActivationObserver& observer;
  bool ran;
  std::size_t molecules;
  std::size_t dissociationFolds;
  double wallTimeS;
};

ptirt::JsonValue MakeLedger(const LedgerInput& in)
{
  using ptirt::JsonValue;
  const auto& config = in.config;
  const bool poisson = config.mode == ptirt::HistoryMode::Poisson;

  auto root = JsonValue::Object();
  root.Set("format", JsonValue::String("ptirt-history-ledger"));
  root.Set("formatVersion", JsonValue::Integer(1));
  root.Set("mode", JsonValue::String(ptirt::HistoryModeName(config.mode)));
  root.Set("config", JsonValue::String(in.arguments.config.string()));
  root.Set("compatibilityTag", JsonValue::String(ptirt::kPacketCompatibilityTag));
  root.Set("geant4Version", JsonValue::String(G4Version));
  root.Set("physics", JsonValue::String("G4EmDNAPhysics_option2"));
  root.Set("chemistry", JsonValue::String("G4EmDNAChemistry_option3"));
  root.Set("chemistryTimeStepModel", JsonValue::String("IRT_syn"));
  root.Set("medium", JsonValue::String("G4_WATER, no bulk scavenger"));
  root.Set("approach", JsonValue::String("separately captured precursor packets injected into one shared "
                                         "standalone IRT_syn history, each source activating at its own time"));
  root.Set("randomEngine", JsonValue::String(G4Random::getTheEngine()->name()));

  auto seeds = JsonValue::Object();
  seeds.Set("chemistrySeed", JsonValue::Integer(static_cast<long long>(config.chemistrySeed)));
  seeds.Set("sourceSeed", poisson ? JsonValue::Integer(static_cast<long long>(config.poisson.sourceSeed))
                                  : JsonValue());
  root.Set("seeds", seeds);

  auto settings = JsonValue::Object();
  settings.Set("libraryDir", JsonValue::String(config.libraryDir.string()));
  settings.Set("scheduleFile", poisson ? JsonValue() : JsonValue::String(config.scheduleFile.string()));
  settings.Set("singleSource", config.mode == ptirt::HistoryMode::Single ? Count(config.singleSource) : JsonValue());
  settings.Set("endpoint_ns", JsonValue::Real(config.endpointNs));
  settings.Set("worldHalfSide_nm", JsonValue::Real(config.worldHalfSideNm));
  settings.Set("reflectingHalfBox_nm", JsonValue::Real(config.halfBoxNm));
  settings.Set("gridMinAge_ns", JsonValue::Real(config.gridMinAgeNs));
  settings.Set("gridPointsPerDecade", JsonValue::Integer(config.gridPointsPerDecade));
  settings.Set("gridGuard_ns", JsonValue::Real(ptirt::kGridGuardNs));
  settings.Set("recordTimeCount", Count(in.grid.size()));
  if (poisson)
  {
    auto process = JsonValue::Object();
    process.Set("rate_Hz", JsonValue::Real(config.poisson.rateHz));
    process.Set("exposure_s", JsonValue::Real(config.poisson.exposureS));
    process.Set("regionCenter_nm", Vector3(config.poisson.regionCenterNm));
    process.Set("regionHalfSize_nm", Vector3(config.poisson.regionHalfSizeNm));
    settings.Set("poisson", process);
  }
  root.Set("settings", settings);

  const auto totals = ptirt::SumLibrary(in.library.entries);
  auto library = JsonValue::Object();
  library.Set("packetCount", Count(totals.packetCount));
  library.Set("moleculeCount", Count(totals.moleculeCount));
  library.Set("primaryEnergy_eV", JsonValue::Real(totals.primaryEnergyEv));
  library.Set("energyDeposit_eV", JsonValue::Real(totals.energyDepositEv));
  library.Set("escapedEnergy_eV", JsonValue::Real(totals.escapedEnergyEv));
  root.Set("library", library);

  // Energies of the injected sources.
  double primary = 0, deposit = 0, escaped = 0;
  auto sources = JsonValue::Array();
  for (const auto& record : in.records)
  {
    const auto& entry = in.library.entries[record.packetIndex];
    primary += entry.primaryEnergyEv;
    deposit += entry.energyDepositEv;
    escaped += entry.escapedEnergyEv;
    auto source = JsonValue::Object();
    source.Set("sourceIndex", Count(record.sourceIndex));
    source.Set("packetIndex", Count(record.packetIndex));
    source.Set("physicsSeed", JsonValue::Integer(entry.physicsSeed));
    source.Set("scheduledTime_ns", JsonValue::Real(record.scheduledTimeNs));
    source.Set("expectedActivation_ns", JsonValue::Real(record.expectedActivationTimeNs));
    source.Set("observedActivation_ns", JsonValue::Real(record.observedActivationTimeNs));
    source.Set("offset_nm", Vector3(record.offsetNm));
    source.Set("moleculeCount", Count(record.moleculeCount));
    source.Set("primaryEnergy_eV", JsonValue::Real(entry.primaryEnergyEv));
    source.Set("energyDeposit_eV", JsonValue::Real(entry.energyDepositEv));
    source.Set("escapedEnergy_eV", JsonValue::Real(entry.escapedEnergyEv));
    sources.Append(source);
  }
  auto injected = JsonValue::Object();
  injected.Set("sourceCount", Count(in.records.size()));
  injected.Set("moleculeCount", Count(in.molecules));
  injected.Set("primaryEnergy_eV", JsonValue::Real(primary));
  injected.Set("energyDeposit_eV", JsonValue::Real(deposit));
  injected.Set("escapedEnergy_eV", JsonValue::Real(escaped));
  root.Set("injected", injected);
  root.Set("sources", sources);

  // Checks: reaching this point means every check passed.
  auto checks = JsonValue::Object();
  checks.Set("allPassed", JsonValue::Bool(true));
  checks.Set("chemistryRan", JsonValue::Bool(in.ran));
  auto stages = JsonValue::Array();
  std::size_t preservedTotal = 0;
  if (in.ran)
    for (const auto& stage : in.observer.Activations().Stages())
    {
      preservedTotal += stage.preserved;
      auto item = JsonValue::Object();
      item.Set("time_ns", JsonValue::Real(stage.timeNs));
      auto labels = JsonValue::Array();
      for (const auto source : stage.sources) labels.Append(Count(in.records[source].sourceIndex));
      item.Set("sources", labels);
      item.Set("injected", Count(stage.injected));
      item.Set("preserved", Count(stage.preserved));
      stages.Append(item);
    }
  auto activation = JsonValue::Object();
  activation.Set("rule", JsonValue::String("each source molecule joins once, at its expected time, with all "
                                           "molecules of its source; nothing else joins"));
  activation.Set("stageCount", Count(in.ran ? in.observer.Activations().Stages().size() : 0));
  activation.Set("stages", stages);
  checks.Set("activation", activation);
  auto preservation = JsonValue::Object();
  preservation.Set("rule", JsonValue::String("every molecule present before an injection is still there, with "
                                             "the same configuration and position, right after it"));
  preservation.Set("preservedTotal", Count(preservedTotal));
  checks.Set("preservation", preservation);
  auto end = JsonValue::Object();
  const auto& e = in.observer.End();
  end.Set("steps", JsonValue::Integer(e.steps));
  end.Set("finalTime_ns", in.ran ? JsonValue::Real(e.finalTime / ns) : JsonValue());
  end.Set("liveMolecules", Count(e.liveMolecules));
  end.Set("liveWater", Count(e.liveWater));
  end.Set("counterSpecies", Count(e.counterSpecies));
  end.Set("allLiveAtEndTime", JsonValue::Bool(!in.ran || e.allLiveAtEndTime));
  end.Set("counterMatchesLive", JsonValue::Bool(!in.ran || e.counterMatchesLive));
  end.Set("allInsideBox", JsonValue::Bool(!in.ran || e.allInsideBox));
  checks.Set("end", end);
  root.Set("checks", checks);

  auto folds = JsonValue::Object();
  folds.Set("dissociationProducts", Count(in.dissociationFolds));
  folds.Set("reactionProducts", Count(in.observer.ProductFoldCount()));
  root.Set("reflectingBoxFolds", folds);
  root.Set("wallTime_s", JsonValue::Real(in.wallTimeS));
  root.Set("outputs", [] {
    auto files = JsonValue::Array();
    for (const char* name : {"species.csv", "activations.csv", "ledger.json"}) files.Append(JsonValue::String(name));
    return files;
  }());
  return root;
}

G4ApplicationState CurrentState()
{
  return G4StateManager::GetStateManager()->GetCurrentState();
}

int Run(int argc, char** argv)
{
  const auto start = std::chrono::steady_clock::now();
  const auto arguments = ParseArguments(argc, argv);

  ptirt::HistoryConfig config;
  try
  {
    config = ptirt::ReadHistoryConfigFile(arguments.config);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, error.what());
  }
  PrepareOutputDirectory(arguments.output);

  ptirt::PacketLibrary library;
  try
  {
    library = ptirt::LoadPacketLibrary(config.libraryDir, ptirt::kPacketCompatibilityTag);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "packet library " + config.libraryDir.string() + ": " + error.what());
  }
  const auto schedule = MakeSchedule(config, library.packets.size());
  std::vector<double> grid;
  try
  {
    grid = ptirt::BuildRecordGrid(schedule.gridArrivalTimesNs, config.endpointNs, config.gridMinAgeNs,
                                  config.gridPointsPerDecade);
  }
  catch (const std::exception& error)
  {
    throw ExitError(2, std::string("record grid: ") + error.what());
  }

  // Declared before the run manager so it outlives it: G4Scheduler keeps a
  // pointer to it and never deletes it.
  ptirt::ActivationObserver observer;

  std::unique_ptr<G4RunManager> runManager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::SerialOnly));
  runManager->SetUserInitialization(new ptirt::HistoryDetectorConstruction(config.worldHalfSideNm * nm));
  auto* physics = new ptirt::ExamplePhysicsList(config.halfBoxNm * nm);
  runManager->SetUserInitialization(physics);

  const G4int counterId = RegisterCounter();
  runManager->Initialize();
  if (CurrentState() != G4State_Idle) throw ExitError(3, "Geant4 is not Idle after initialization");
  if (const auto problem = ptirt::DescribeIrtSynParameterProblem(); !problem.empty()) throw ExitError(4, problem);

  // Validates every packet against this build before any Geant4 state changes.
  std::unique_ptr<ptirt::SourceInjector> injector;
  try
  {
    injector = std::make_unique<ptirt::SourceInjector>(schedule.arrivals, library, config.worldHalfSideNm * nm,
                                                       config.halfBoxNm * nm, config.endpointNs * ns);
    observer.Configure(injector->Sources(), grid, physics->GetReflectingBox(), counterId);
  }
  catch (const std::exception& error)
  {
    throw ExitError(2, error.what());
  }

  auto* scheduler = G4Scheduler::Instance();
  scheduler->SetUserAction(&observer);
  scheduler->SetVerbose(0);
  scheduler->SetEndTime(config.endpointNs * ns);
  injector->Inject();

  // The history's own random stream: the chemistry seed only.
  G4Random::setTheSeed(static_cast<long>(config.chemistrySeed));
  auto* chemistry = G4DNAChemistryManager::Instance();
  chemistry->UseAsStandalone(true);
  const bool ran = !injector->Sources().empty();
  if (ran)
    chemistry->Run();
  else
    observer.FinishEmpty();  // no source (e.g. a Poisson draw of zero): zero counts on the grid

  if (!observer.ModelProblem().empty()) throw ExitError(4, observer.ModelProblem());
  if (!observer.Failure().empty()) throw ExitError(1, observer.Failure());
  if (!observer.Finished()) throw ExitError(1, "the chemistry history ended without its end-of-history checks");

  auto records = injector->MakeActivationRecords();
  if (ran)
    for (std::size_t k = 0; k < records.size(); ++k)
      records[k].observedActivationTimeNs = observer.Activations().ObservedActivationTimeNs(k);

  auto rows = observer.SpeciesRows();
  for (auto& row : rows)
  {
    row.history = ptirt::HistoryModeName(config.mode);
    row.chemistrySeed = config.chemistrySeed;
  }
  WriteFile(arguments.output / "species.csv", [&](std::ostream& out) { ptirt::WriteSpeciesCsv(out, rows, true); });
  WriteFile(arguments.output / "activations.csv",
            [&](std::ostream& out) { ptirt::WriteActivationsCsv(out, records); });

  const double wallTimeS = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const LedgerInput input{arguments, config,    library,   schedule, grid, records, observer, ran,
                          injector->MoleculeCount(), physics->GetDissociationFoldCount(), wallTimeS};
  WriteFile(arguments.output / "ledger.json", [&](std::ostream& out) {
    ptirt::WriteJson(out, MakeLedger(input));
    out << '\n';
  });

  std::ostringstream done;
  done.precision(17);
  done << "HISTORY_DONE mode=" << ptirt::HistoryModeName(config.mode) << " sources=" << records.size()
       << " molecules=" << injector->MoleculeCount()
       << " stages=" << (ran ? observer.Activations().Stages().size() : 0) << " steps=" << observer.End().steps
       << " records=" << rows.size() << " chemistrySeed=" << config.chemistrySeed
       << " output=" << arguments.output.string();
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
    std::cerr << "HISTORY_ERROR " << error.what() << std::endl;
    return error.Status();
  }
  catch (const std::exception& error)
  {
    std::cerr << "HISTORY_ERROR " << error.what() << std::endl;
    return 1;
  }
}
