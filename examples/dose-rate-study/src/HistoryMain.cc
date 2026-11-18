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
//   poisson        a Poisson arrival process drawn from sourceSeed;
//   doseCutoff     a Poisson process at the rate of a dose rate, from t = 0,
//                  until the energy deposited in the capture box reaches a
//                  dose; that arrival is beam-off and the chemistry ends just
//                  after it (or earlier, at a steady-state stop). See
//                  ArrivalSchedule.hh "Dose cutoff", TimeGrid.hh and
//                  SteadyStateDetector.hh.
// The chemistry seed (chemistrySeed) is separate from the source seed.
//
// Output in <dir> (created if missing, must be empty):
//   species.csv      molecule counts per species on the record grid, absolute
//                    time (timeNs,species,count,history,chemistrySeed); in
//                    doseCutoff mode, on the common log grid with E(t), the
//                    dose, both G-values and the extrapolated flag
//                    (WriteDoseSpeciesCsv, Ledger.hh);
//   species_age.csv  doseCutoff only: the counts against the age since each
//                    arrival (WriteAgeSpeciesCsv);
//   activations.csv  one row per source: scheduled, expected and observed
//                    activation times, track IDs, energies;
//   reactions.csv    one row per two-molecule reaction: time, reactants with
//                    the source each came from, products (ReactionLog.hh);
//   snapshots.h5     doseCutoff with `snapshots = true` only (needs HDF5): the
//                    position and species of every live molecule at 3 times
//                    per decade of the common grid (SnapshotWriter.hh; read it
//                    with scripts/snapshots.py, see scripts/movie.py);
//   ledger.json      seeds, settings, library and source energies, the checks;
//                    in doseCutoff mode also the rates, the dose cutoff, the
//                    grids, the steady state and the energy checks.
// Exit status 0 and a "HISTORY_DONE" line on success; otherwise a
// "HISTORY_ERROR <reason>" line on stderr and a nonzero status: 2 bad
// arguments, config, schedule or packets; 3 initialization failure; 4 the
// chemistry model is not IRT_syn; 1 a failed check, or a failure during the
// history or while writing the outputs.
#include <algorithm>
#include <chrono>
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

#include "ActivationObserver.hh"
#include "ArrivalSchedule.hh"
#include "CaptureLibrary.hh"
#include "DrsPhysicsList.hh"
#include "DrsPhysicsTag.hh"
#include "HistoryConfig.hh"
#include "HistoryDetectorConstruction.hh"
#include "Ledger.hh"
#include "SnapshotWriter.hh"
#include "SourceInjector.hh"
#include "SteadyStateDetector.hh"
#include "TimeGrid.hh"

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
  std::vector<drs::Arrival> arrivals;
  std::vector<double> gridArrivalTimesNs;
};

std::vector<drs::Arrival> ReadScheduleFile(const std::filesystem::path& file, const drs::ScheduleBounds& bounds)
{
  std::ifstream in(file);
  if (!in) throw ExitError(2, "cannot open schedule " + file.string());
  try
  {
    return drs::ParseDeterministicSchedule(in, bounds);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "schedule " + file.string() + ": " + error.what());
  }
}

Schedule MakeSchedule(const drs::HistoryConfig& config, std::size_t libraryPacketCount)
{
  drs::ScheduleBounds bounds;
  bounds.libraryPacketCount = libraryPacketCount;
  bounds.endpointNs = config.endpointNs;

  Schedule schedule;
  switch (config.mode)
  {
    case drs::HistoryMode::Deterministic:
      schedule.arrivals = ReadScheduleFile(config.scheduleFile, bounds);
      break;
    case drs::HistoryMode::Single:
    {
      const auto full = ReadScheduleFile(config.scheduleFile, bounds);
      if (config.singleSource >= full.size())
        throw ExitError(2, "singleSource " + std::to_string(config.singleSource) + " is not a row of the " +
                               std::to_string(full.size()) + "-row schedule " + config.scheduleFile.string());
      for (const auto& arrival : full) schedule.gridArrivalTimesNs.push_back(arrival.timeNs);
      schedule.arrivals = {full[config.singleSource]};
      return schedule;
    }
    case drs::HistoryMode::Poisson:
      try
      {
        schedule.arrivals = drs::SamplePoissonArrivals(config.poisson, bounds);
      }
      catch (const std::runtime_error& error)
      {
        throw ExitError(2, std::string("poisson schedule: ") + error.what());
      }
      break;
    case drs::HistoryMode::DoseCutoff:
      break;  // PlanDoseCutoff: the arrivals need the capture summary
  }
  for (const auto& arrival : schedule.arrivals) schedule.gridArrivalTimesNs.push_back(arrival.timeNs);
  return schedule;
}

// ---- doseCutoff ---------------------------------------------------------------

// Everything a doseCutoff history is built on, decided before Geant4 starts.
struct DosePlan
{
  drs::CaptureSummary capture;
  drs::ArrivalRates rates;
  drs::DoseCutoffSchedule schedule;
  std::vector<double> activationNs;  // per arrival: arrival time + packet capture age
  std::vector<double> depositsEv;    // per arrival: the packet's box deposit
  double beamOffNs = 0;              // last activation + kBeamOffMarginNs: the chemistry end
  std::vector<double> commonGridNs;
  std::vector<drs::AgePoint> agePoints;
};

// Rates and schedule: needs library.csv and capture.json only.
DosePlan PlanDoseCutoff(const drs::HistoryConfig& config, const drs::PacketLibrary& library)
{
  DosePlan plan;
  try
  {
    plan.capture = drs::LoadCaptureSummary(config.libraryDir);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "packet library " + config.libraryDir.string() + ": " + error.what());
  }
  // The reflecting box is the dose box: the molecules and the deposit belong to it.
  const double captureNm = plan.capture.captureHalfSideNm;
  if (!(std::abs(config.halfBoxNm - captureNm) <= 1e-6 * captureNm))
  {
    std::ostringstream text;
    text.precision(17);
    text << "halfBoxUm (" << config.halfBoxNm / 1000.0 << " um) must be the capture box of the library ("
         << captureNm / 1000.0 << " um, capture.json captureHalfSide_nm): the dose is the energy deposited in it";
    throw ExitError(2, text.str());
  }
  std::vector<double> deposits;
  deposits.reserve(library.entries.size());
  for (const auto& entry : library.entries) deposits.push_back(entry.energyDepositEv);
  double emptySum = 0;
  for (const auto& empty : plan.capture.skippedEmptyEvents) emptySum += empty.energyDepositEv;
  try
  {
    plan.rates = drs::ComputeArrivalRates(config.dose.doseRateGyPerS, config.dose.doseGy, captureNm, deposits,
                                          plan.capture.skippedEmptyEvents.size(), emptySum);
    drs::DoseCutoffParameters parameters;
    parameters.rateHz = plan.rates.packetRateHz;
    parameters.targetEnergyEv = plan.rates.targetEnergyEv;
    parameters.sourceSeed = config.dose.sourceSeed;
    parameters.firstPacket = config.dose.firstPacket;
    plan.schedule = drs::SampleDoseCutoffArrivals(parameters, deposits, config.dose.maxArrivals);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, std::string("doseCutoff schedule: ") + error.what());
  }
  return plan;
}

// Activations, beam-off and grids: needs the scheduled packets loaded.
void FinishDosePlan(DosePlan& plan, const drs::HistoryConfig& config, const drs::PacketLibrary& library)
{
  std::vector<double> arrivalNs;
  double lastActivation = 0;
  for (const auto& arrival : plan.schedule.arrivals)
  {
    const double activation = arrival.timeNs + library.packets[arrival.packetIndex].captureTimeNs;
    arrivalNs.push_back(arrival.timeNs);
    plan.activationNs.push_back(activation);
    plan.depositsEv.push_back(library.entries[arrival.packetIndex].energyDepositEv);
    lastActivation = std::max(lastActivation, activation);
  }
  plan.beamOffNs = lastActivation + drs::kBeamOffMarginNs;
  try
  {
    plan.commonGridNs = drs::BuildCommonLogGrid(drs::kCommonGridStartNs, drs::kCommonGridPointsPerDecade,
                                                plan.beamOffNs);
    if (config.dose.ageGridPointsPerDecade > 0)
      plan.agePoints = drs::BuildAgeGrids(arrivalNs, drs::kCommonGridStartNs, config.dose.ageGridPointsPerDecade,
                                          plan.beamOffNs);
  }
  catch (const std::exception& error)
  {
    throw ExitError(2, std::string("doseCutoff record grids: ") + error.what());
  }
}

// The observer's record times: the common grid and every age point.
std::vector<double> DoseRecordTimes(const DosePlan& plan)
{
  std::vector<double> times = plan.commonGridNs;
  for (const auto& point : plan.agePoints) times.push_back(point.timeNs);
  std::sort(times.begin(), times.end());
  times.erase(std::unique(times.begin(), times.end()), times.end());
  return times;
}

// What the doseCutoff history produced, for the outputs and the ledger.
struct DoseOutcome
{
  std::vector<drs::DoseSpeciesRow> doseRows;
  std::vector<drs::AgeSpeciesRow> ageRows;
  std::vector<double> steadyCounts;  // N_ss per species after a steady-state stop, else empty
  std::vector<std::string> species;
  double finalEnergyEv = 0;
  double energyAtBeamOffEv = 0;
  double injectedDepositEv = 0;
  std::size_t extrapolatedTimes = 0;  // common-grid times filled with N_ss
  std::size_t measuredAgePoints = 0;
  drs::DoseEnergyChecks checks;
};

DoseOutcome MakeDoseOutcome(const DosePlan& plan, const drs::ActivationObserver& observer,
                            const std::vector<drs::ActivationRecord>& records)
{
  DoseOutcome outcome;
  const auto& table = observer.MeasuredTable();
  outcome.species = table.species;
  if (observer.StoppedEarly() && observer.SteadyState() != nullptr)
    outcome.steadyCounts = observer.SteadyState()->SteadyMeans();

  // E(t) from the expected activations, sorted (they already are when the
  // capture ages are equal).
  std::vector<std::pair<double, double>> sources;
  for (std::size_t k = 0; k < plan.activationNs.size(); ++k)
    sources.emplace_back(plan.activationNs[k], plan.depositsEv[k]);
  std::sort(sources.begin(), sources.end());
  std::vector<double> activations, deposits;
  for (const auto& [activation, deposit] : sources)
  {
    activations.push_back(activation);
    deposits.push_back(deposit);
  }
  const drs::CumulativeEnergy energy(activations, deposits);

  outcome.finalEnergyEv = plan.schedule.cumulativeEnergyEv.back();
  outcome.energyAtBeamOffEv = energy.At(plan.beamOffNs);
  for (const auto& record : records) outcome.injectedDepositEv += record.energyDepositEv;
  outcome.checks = drs::CheckDoseEnergy(plan.schedule, plan.rates.targetEnergyEv, outcome.energyAtBeamOffEv,
                                        outcome.injectedDepositEv);

  const drs::DoseNormalization normalization{outcome.finalEnergyEv, plan.rates.boxMassKg};
  std::vector<double> gridEnergy;
  for (const double t : plan.commonGridNs) gridEnergy.push_back(energy.At(t));
  std::vector<double> ageEnergy;
  for (const auto& point : plan.agePoints) ageEnergy.push_back(energy.At(point.timeNs));
  try
  {
    outcome.doseRows = drs::MakeDoseRows(plan.commonGridNs, gridEnergy, table, outcome.steadyCounts, normalization);
    outcome.ageRows = drs::MakeAgeRows(plan.agePoints, ageEnergy, table, normalization);
  }
  catch (const std::exception& error)
  {
    throw ExitError(1, std::string("doseCutoff species rows: ") + error.what());
  }
  for (const double t : plan.commonGridNs)
    if (table.countsAt.count(t) == 0) ++outcome.extrapolatedTimes;
  for (const auto& point : plan.agePoints)
    if (table.countsAt.count(point.timeNs) > 0) ++outcome.measuredAgePoints;
  return outcome;
}

drs::JsonValue Count(std::size_t n)
{
  return drs::JsonValue::Integer(static_cast<long long>(n));
}

drs::JsonValue Reals(const std::vector<double>& values)
{
  auto array = drs::JsonValue::Array();
  for (const double x : values) array.Append(drs::JsonValue::Real(x));
  return array;
}

// The doseCutoff sections of ledger.json.
void AddDoseLedger(drs::JsonValue& root, drs::JsonValue& checks, const drs::HistoryConfig& config,
                   const DosePlan& plan, const DoseOutcome& outcome, const drs::ActivationObserver& observer)
{
  using drs::JsonValue;
  const auto& r = plan.rates;
  auto rates = JsonValue::Object();
  rates.Set("doseRate_Gy_per_s", JsonValue::Real(r.doseRateGyPerS));
  rates.Set("boxHalfSide_nm", JsonValue::Real(r.boxHalfSideNm));
  rates.Set("waterDensity_kg_per_m3", JsonValue::Real(drs::kWaterDensityKgPerM3));
  rates.Set("boxMass_kg", JsonValue::Real(r.boxMassKg));
  rates.Set("packetCount", Count(r.packetCount));
  rates.Set("emptyCount", Count(r.emptyCount));
  rates.Set("launchedCount", Count(r.launchedCount));
  rates.Set("hitFraction", JsonValue::Real(r.hitFraction));
  rates.Set("packetDepositSum_eV", JsonValue::Real(r.packetDepositSumEv));
  rates.Set("emptyDepositSum_eV", JsonValue::Real(r.emptyDepositSumEv));
  rates.Set("meanDepositPerLaunched_eV", JsonValue::Real(r.meanDepositPerLaunchedEv));
  rates.Set("meanDepositPerPacket_eV", JsonValue::Real(r.meanDepositPerPacketEv));
  rates.Set("launchedRate_Hz", JsonValue::Real(r.launchedRateHz));
  rates.Set("packetRate_Hz", JsonValue::Real(r.packetRateHz));
  rates.Set("emptyCrossings",
            JsonValue::String("thinning: an empty crossing (no packet) is not scheduled and counts as 0 eV; "
                              "packets arrive at packetRate_Hz = hitFraction * launchedRate_Hz, launchedRate_Hz = "
                              "doseRate * boxMass / meanDepositPerLaunched; emptyDepositSum_eV is reported only"));
  root.Set("rates", rates);

  const auto& arrivals = plan.schedule.arrivals;
  const double finalGy = outcome.finalEnergyEv * drs::kJoulePerEv / r.boxMassKg;
  auto dose = JsonValue::Object();
  dose.Set("target_Gy", JsonValue::Real(r.targetDoseGy));
  dose.Set("targetEnergy_eV", JsonValue::Real(r.targetEnergyEv));
  dose.Set("finalEnergy_eV", JsonValue::Real(outcome.finalEnergyEv));
  dose.Set("final_Gy", JsonValue::Real(finalGy));
  dose.Set("overshoot_eV", JsonValue::Real(outcome.finalEnergyEv - r.targetEnergyEv));
  dose.Set("cutoffTrackDeposit_eV", JsonValue::Real(plan.depositsEv.back()));
  dose.Set("arrivalCount", Count(arrivals.size()));
  dose.Set("firstPacket", Count(config.dose.firstPacket));
  dose.Set("lastPacket", Count(arrivals.back().packetIndex));
  dose.Set("maxArrivals", Count(config.dose.maxArrivals));
  dose.Set("cutoffArrival", Count(arrivals.back().sourceIndex));
  dose.Set("cutoffArrivalTime_ns", JsonValue::Real(arrivals.back().timeNs));
  dose.Set("cutoffActivation_ns", JsonValue::Real(plan.activationNs.back()));
  dose.Set("beamOffMargin_ns", JsonValue::Real(drs::kBeamOffMarginNs));
  dose.Set("beamOff_ns", JsonValue::Real(plan.beamOffNs));
  dose.Set("chemistryEnd", JsonValue::String("beam-off (no post-beam relaxation), or the steady-state stop"));
  dose.Set("effectiveDoseRate_Gy_per_s", JsonValue::Real(finalGy / (plan.beamOffNs * 1e-9)));
  root.Set("dose", dose);

  auto grids = JsonValue::Object();
  grids.Set("commonStart_ns", JsonValue::Real(drs::kCommonGridStartNs));
  grids.Set("commonPointsPerDecade", JsonValue::Integer(drs::kCommonGridPointsPerDecade));
  grids.Set("commonPointCount", Count(plan.commonGridNs.size()));
  grids.Set("commonRule", JsonValue::String("t_k = 1 ps * 10^(k/10), the same for every dose rate, below "
                                            "beam-off, then beam-off"));
  grids.Set("ageStart_ns", JsonValue::Real(drs::kCommonGridStartNs));
  grids.Set("agePointsPerDecade", JsonValue::Integer(config.dose.ageGridPointsPerDecade));
  grids.Set("agePointCount", Count(plan.agePoints.size()));
  grids.Set("measuredAgePointCount", Count(outcome.measuredAgePoints));
  grids.Set("counterPrecision_ns", JsonValue::Real(drs::kCounterPrecisionNs));
  root.Set("grids", grids);

  const auto* detector = observer.SteadyState();
  const auto& settings = config.dose.steadyState;
  const bool stopped = observer.StoppedEarly();
  auto steady = JsonValue::Object();
  steady.Set("enabled", JsonValue::Bool(config.dose.steadyStateWindow > 0));
  steady.Set("window", Count(config.dose.steadyStateWindow));
  steady.Set("tolerance", JsonValue::Real(settings.tolerance));
  steady.Set("minMean", JsonValue::Real(settings.minMean));
  steady.Set("rule", JsonValue::String("one sample per arrival after the first (live molecules per species just "
                                       "before its source joins); consecutive windows of `window` samples; stop "
                                       "when |mean_now - mean_before| < tolerance * mean_before for every species "
                                       "whose means are not both <= minMean; the later common-grid rows get N_ss "
                                       "= the last window mean, extrapolated = 1"));
  steady.Set("reached", JsonValue::Bool(stopped));
  steady.Set("sampleCount", Count(detector != nullptr ? detector->SampleCount() : 0));
  steady.Set("windowMaxRelativeChanges",
             detector != nullptr ? Reals(detector->WindowMaxRelativeChanges()) : JsonValue::Array());
  steady.Set("stopTime_ns", stopped ? JsonValue::Real(observer.End().checkTime / ns) : JsonValue());
  steady.Set("activatedSourcesAtStop",
             stopped ? Count(observer.Activations().Sources().size() - observer.Activations().PendingSourceCount())
                     : JsonValue());
  auto steadyCounts = JsonValue::Object();
  for (std::size_t k = 0; k < outcome.steadyCounts.size(); ++k)
    steadyCounts.Set(outcome.species[k], JsonValue::Real(outcome.steadyCounts[k]));
  steady.Set("steadyCounts", steadyCounts);
  steady.Set("extrapolatedTimeCount", Count(outcome.extrapolatedTimes));
  steady.Set("extrapolatedRowCount", Count(outcome.extrapolatedTimes * outcome.species.size()));
  root.Set("steadyState", steady);

  auto snapshots = JsonValue::Object();
  snapshots.Set("enabled", JsonValue::Bool(observer.SnapshotsEnabled()));
  snapshots.Set("perDecade", JsonValue::Integer(config.dose.snapshotsPerDecade));
  snapshots.Set("requestedTimeCount", Count(observer.SnapshotTimesRequested()));
  snapshots.Set("droppedBeforeFirstActivation", Count(observer.SnapshotTimesDropped()));
  snapshots.Set("mergedIntoAnotherFrame", Count(observer.SnapshotTimesMerged()));
  snapshots.Set("compressed", JsonValue::Bool(drs::SnapshotWriter::Compressed()));
  snapshots.Set("frameCount", Count(observer.SnapshotFrameTimesNs().size()));
  snapshots.Set("moleculeCount", Count(observer.SnapshotMoleculeTotal()));
  snapshots.Set("rule", JsonValue::String("the common-grid points closest to 1 ps * 10^(s/perDecade), then beam-off; "
                                          "frame time_ns is the clock time of the frame (a requested time within "
                                          "0.001 ns of an activation or the end is taken at that stage boundary); "
                                          "the history is stepped to each snapshot time, so its random stream "
                                          "differs from a run without snapshots"));
  root.Set("snapshots", snapshots);

  const auto& c = outcome.checks;
  auto energy = JsonValue::Object();
  energy.Set("rule", JsonValue::String("E_final >= target > E_final - (cutoff deposit); E(beam-off) and the "
                                       "injected deposits both equal E_final"));
  energy.Set("allPassed", JsonValue::Bool(c.AllPassed()));
  energy.Set("targetReached", JsonValue::Bool(c.targetReached));
  energy.Set("overshootBelowOneTrack", JsonValue::Bool(c.overshootBelowOneTrack));
  energy.Set("beamOffHoldsEverySource", JsonValue::Bool(c.beamOffHoldsEverySource));
  energy.Set("injectedMatchesSchedule", JsonValue::Bool(c.injectedMatchesSchedule));
  energy.Set("energyAtBeamOff_eV", JsonValue::Real(outcome.energyAtBeamOffEv));
  energy.Set("injectedDeposit_eV", JsonValue::Real(outcome.injectedDepositEv));
  checks.Set("energy", energy);
  checks.Set("allPassed", JsonValue::Bool(c.AllPassed()));
}

// Registered before /run/initialize, like dnachem-min: G4DNAChemistryManager
// initializes the counters it finds. One counter per species configuration,
// H2O ignored, times within kCounterPrecisionNs (1 fs) merged: a source counts
// at a record time t when its activation - t < 1 fs, the rule CumulativeEnergy
// uses for E(t) (TimeGrid.hh). Reset before each event: there is no event
// here, so the counter only holds this history.
G4int RegisterCounter()
{
  auto* manager = G4MoleculeCounterManager::Instance();
  manager->SetResetCountersBeforeEvent(true);
  auto counter = std::make_unique<G4MoleculeCounter>("BasicCounter");
  counter->IgnoreMolecule(G4H2O::Definition());
  counter->SetTimeComparer(G4MoleculeCounterTimeComparer::CreateWithFixedPrecision(drs::kCounterPrecisionNs * ns));
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

drs::JsonValue Vector3(const std::array<double, 3>& v)
{
  auto array = drs::JsonValue::Array();
  for (double x : v) array.Append(drs::JsonValue::Real(x));
  return array;
}

struct LedgerInput
{
  const Arguments& arguments;
  const drs::HistoryConfig& config;
  const drs::PacketLibrary& library;
  const Schedule& schedule;
  const std::vector<double>& grid;
  const std::vector<drs::ActivationRecord>& records;
  const drs::ActivationObserver& observer;
  bool ran;
  std::size_t molecules;
  std::size_t dissociationFolds;
  double wallTimeS;
  const DosePlan* dosePlan = nullptr;        // doseCutoff only
  const DoseOutcome* doseOutcome = nullptr;  // doseCutoff only
};

drs::JsonValue MakeLedger(const LedgerInput& in)
{
  using drs::JsonValue;
  const auto& config = in.config;
  const bool poisson = config.mode == drs::HistoryMode::Poisson;
  const bool dose = config.mode == drs::HistoryMode::DoseCutoff;
  const bool scheduleFile =
      config.mode == drs::HistoryMode::Deterministic || config.mode == drs::HistoryMode::Single;

  auto root = JsonValue::Object();
  root.Set("format", JsonValue::String("drs-history-ledger"));
  root.Set("formatVersion", JsonValue::Integer(dose ? 2 : 1));
  root.Set("mode", JsonValue::String(drs::HistoryModeName(config.mode)));
  root.Set("config", JsonValue::String(in.arguments.config.string()));
  root.Set("compatibilityTag", JsonValue::String(drs::kPacketCompatibilityTag));
  root.Set("geant4Version", JsonValue::String(G4Version));
  root.Set("physics", JsonValue::String("G4EmDNAPhysics_option2"));
  root.Set("chemistry", JsonValue::String("PureWater (copied network + acid-base buffer), pH 7, IRT_syn"));
  root.Set("chemistryTimeStepModel", JsonValue::String("IRT_syn"));
  // Built against a kernel with RegisterSourceActivation (source-contacts patch).
  root.Set("sourceActivation", JsonValue::Bool(DRS_HAS_SOURCE_ACTIVATION != 0));
  root.Set("medium", JsonValue::String("G4_WATER, pH 7 acid-base buffer (H3Op(B), OHm(B)), no O2"));
  root.Set("approach", JsonValue::String("separately captured precursor packets injected into one shared "
                                         "standalone IRT_syn history, each source activating at its own time"));
  root.Set("randomEngine", JsonValue::String(G4Random::getTheEngine()->name()));

  auto seeds = JsonValue::Object();
  seeds.Set("chemistrySeed", JsonValue::Integer(static_cast<long long>(config.chemistrySeed)));
  seeds.Set("sourceSeed", poisson ? JsonValue::Integer(static_cast<long long>(config.poisson.sourceSeed))
                          : dose  ? JsonValue::Integer(static_cast<long long>(config.dose.sourceSeed))
                                  : JsonValue());
  root.Set("seeds", seeds);

  auto settings = JsonValue::Object();
  settings.Set("libraryDir", JsonValue::String(config.libraryDir.string()));
  settings.Set("scheduleFile", scheduleFile ? JsonValue::String(config.scheduleFile.string()) : JsonValue());
  settings.Set("singleSource", config.mode == drs::HistoryMode::Single ? Count(config.singleSource) : JsonValue());
  settings.Set("endpoint_ns", JsonValue::Real(config.endpointNs));
  settings.Set("worldHalfSide_nm", JsonValue::Real(config.worldHalfSideNm));
  settings.Set("reflectingHalfBox_nm", JsonValue::Real(config.halfBoxNm));
  // doseCutoff: the grids are in the "grids" section; the endpoint is beam-off.
  settings.Set("gridMinAge_ns", dose ? JsonValue() : JsonValue::Real(config.gridMinAgeNs));
  settings.Set("gridPointsPerDecade", dose ? JsonValue() : JsonValue::Integer(config.gridPointsPerDecade));
  settings.Set("gridGuard_ns", dose ? JsonValue() : JsonValue::Real(drs::kGridGuardNs));
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

  const auto totals = drs::SumLibrary(in.library.entries);
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
  end.Set("stoppedEarly", JsonValue::Bool(e.stoppedEarly));
  end.Set("checkTime_ns", in.ran ? JsonValue::Real(e.checkTime / ns) : JsonValue());
  end.Set("pendingSourceMolecules", Count(e.pendingSourceMolecules));
  checks.Set("end", end);
  if (dose) AddDoseLedger(root, checks, config, *in.dosePlan, *in.doseOutcome, in.observer);
  root.Set("checks", checks);

  auto folds = JsonValue::Object();
  folds.Set("dissociationProducts", Count(in.dissociationFolds));
  folds.Set("reactionProducts", Count(in.observer.ProductFoldCount()));
  root.Set("reflectingBoxFolds", folds);
  auto reactions = JsonValue::Object();
  reactions.Set("logged", Count(in.observer.Reactions().Records().size()));
  reactions.Set("interSource", Count(in.observer.Reactions().InterSourceCount()));
  reactions.Set("unknownOriginReactants", Count(in.observer.Reactions().UnknownCount()));
  reactions.Set("bulkProductsMatchedByPosition", Count(in.observer.Reactions().PositionMatchedCount()));
  reactions.Set("bulkReactions", Count(in.observer.Reactions().BulkCount()));
  root.Set("reactions", reactions);
  root.Set("wallTime_s", JsonValue::Real(in.wallTimeS));
  root.Set("outputs", [dose, snapshots = in.observer.SnapshotsEnabled()] {
    auto files = JsonValue::Array();
    files.Append(JsonValue::String("species.csv"));
    if (dose) files.Append(JsonValue::String("species_age.csv"));
    if (snapshots) files.Append(JsonValue::String("snapshots.h5"));
    for (const char* name : {"activations.csv", "reactions.csv", "ledger.json"}) files.Append(JsonValue::String(name));
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

  drs::HistoryConfig config;
  try
  {
    config = drs::ReadHistoryConfigFile(arguments.config);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, error.what());
  }
  PrepareOutputDirectory(arguments.output);

  drs::PacketLibrary library;
  try
  {
    library = drs::LoadLibraryIndex(config.libraryDir);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "packet library " + config.libraryDir.string() + ": " + error.what());
  }
  const bool dose = config.mode == drs::HistoryMode::DoseCutoff;
  const bool snapshots = dose && config.dose.snapshots;
  if (snapshots && !drs::SnapshotWriter::Available())
    throw ExitError(2, "config: snapshots = true needs HDF5, and this build has none (configure with "
                       "-DDRS_WITH_HDF5=AUTO or ON)");
  auto schedule = MakeSchedule(config, library.entries.size());
  DosePlan plan;
  if (dose)
  {
    plan = PlanDoseCutoff(config, library);
    schedule.arrivals = plan.schedule.arrivals;
    for (const auto& arrival : schedule.arrivals) schedule.gridArrivalTimesNs.push_back(arrival.timeNs);
  }
  try
  {
    // Only the packets this history injects are read (and checked against library.csv).
    std::vector<std::size_t> used;
    used.reserve(schedule.arrivals.size());
    for (const auto& arrival : schedule.arrivals) used.push_back(arrival.packetIndex);
    drs::LoadPackets(library, config.libraryDir, used, drs::kPacketCompatibilityTag);
  }
  catch (const std::runtime_error& error)
  {
    throw ExitError(2, "packet library " + config.libraryDir.string() + ": " + error.what());
  }
  std::vector<double> grid;
  if (dose)
  {
    // Beam-off is the chemistry end: the endpoint every later step uses.
    FinishDosePlan(plan, config, library);
    config.endpointNs = plan.beamOffNs;
    grid = DoseRecordTimes(plan);
  }
  else
    try
    {
      grid = drs::BuildRecordGrid(schedule.gridArrivalTimesNs, config.endpointNs, config.gridMinAgeNs,
                                    config.gridPointsPerDecade);
    }
    catch (const std::exception& error)
    {
      throw ExitError(2, std::string("record grid: ") + error.what());
    }

  // Declared before the run manager so it outlives it: G4Scheduler keeps a
  // pointer to it and never deletes it.
  drs::ActivationObserver observer;

  std::unique_ptr<G4RunManager> runManager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::SerialOnly));
  runManager->SetUserInitialization(new drs::HistoryDetectorConstruction(config.worldHalfSideNm * nm));
  auto* physics = new drs::DrsPhysicsList(config.halfBoxNm * nm);
  runManager->SetUserInitialization(physics);

  const G4int counterId = RegisterCounter();
  runManager->Initialize();
  if (CurrentState() != G4State_Idle) throw ExitError(3, "Geant4 is not Idle after initialization");
  if (const auto problem = drs::DescribeIrtSynParameterProblem(); !problem.empty()) throw ExitError(4, problem);

  // Validates every packet against this build before any Geant4 state changes.
  std::unique_ptr<drs::SourceInjector> injector;
  try
  {
    injector = std::make_unique<drs::SourceInjector>(schedule.arrivals, library, config.worldHalfSideNm * nm,
                                                       config.halfBoxNm * nm, config.endpointNs * ns);
    observer.Configure(injector->Sources(), grid, physics->GetReflectingBox(), counterId);
    observer.SetProgressInterval(config.progressIntervalS);
    if (dose && config.dose.steadyStateWindow > 0) observer.EnableSteadyState(config.dose.steadyState);
    if (snapshots)
      observer.EnableSnapshots(arguments.output / "snapshots.h5",
                               drs::SelectSnapshotTimes(plan.commonGridNs, drs::kCommonGridPointsPerDecade,
                                                        config.dose.snapshotsPerDecade));
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

  observer.CloseSnapshots();
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
    row.history = drs::HistoryModeName(config.mode);
    row.chemistrySeed = config.chemistrySeed;
  }
  DoseOutcome outcome;
  std::size_t recordCount = rows.size();
  if (dose)
  {
    outcome = MakeDoseOutcome(plan, observer, records);
    recordCount = outcome.doseRows.size();
    WriteFile(arguments.output / "species.csv",
              [&](std::ostream& out) { drs::WriteDoseSpeciesCsv(out, outcome.doseRows); });
    WriteFile(arguments.output / "species_age.csv",
              [&](std::ostream& out) { drs::WriteAgeSpeciesCsv(out, outcome.ageRows); });
  }
  else
    WriteFile(arguments.output / "species.csv", [&](std::ostream& out) { drs::WriteSpeciesCsv(out, rows, true); });
  WriteFile(arguments.output / "activations.csv",
            [&](std::ostream& out) { drs::WriteActivationsCsv(out, records); });
  WriteFile(arguments.output / "reactions.csv",
            [&](std::ostream& out) { drs::WriteReactionsCsv(out, observer.Reactions().Records()); });

  const double wallTimeS = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  LedgerInput input{arguments, config,    library,   schedule, grid, records, observer, ran,
                    injector->MoleculeCount(), physics->GetDissociationFoldCount(), wallTimeS};
  if (dose)
  {
    input.dosePlan = &plan;
    input.doseOutcome = &outcome;
  }
  WriteFile(arguments.output / "ledger.json", [&](std::ostream& out) {
    drs::WriteJson(out, MakeLedger(input));
    out << '\n';
  });
  // The outputs are written first, so a failed energy check can be inspected.
  if (dose && !outcome.checks.AllPassed())
    throw ExitError(1, "doseCutoff energy checks failed (ledger.json checks.energy)");

  std::ostringstream done;
  done.precision(17);
  done << "HISTORY_DONE mode=" << drs::HistoryModeName(config.mode) << " sources=" << records.size()
       << " molecules=" << injector->MoleculeCount()
       << " stages=" << (ran ? observer.Activations().Stages().size() : 0) << " steps=" << observer.End().steps
       << " records=" << recordCount << " chemistrySeed=" << config.chemistrySeed;
  if (dose)
    done << " stoppedEarly=" << (observer.StoppedEarly() ? 1 : 0) << " endTime_ns=" << config.endpointNs;
  if (snapshots) done << " snapshotFrames=" << observer.SnapshotFrameTimesNs().size();
  done << " reactions=" << observer.Reactions().Records().size()
       << " interSource=" << observer.Reactions().InterSourceCount()
       << " unknownOrigin=" << observer.Reactions().UnknownCount()
       << " positionMatched=" << observer.Reactions().PositionMatchedCount()
       << " bulk=" << observer.Reactions().BulkCount();
  done << " wallTime_s=" << wallTimeS << " output=" << arguments.output.string();
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
