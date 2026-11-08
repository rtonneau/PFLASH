#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "BeamSchedule.hh"
#include "FiniteSchedule.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4H2O.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeCounter.hh"
#include <filesystem>
#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>

namespace {
class BeamAction final : public HistoryAction {
public:
  std::set<int> pending;
  std::size_t registered = 0;
  std::vector<std::pair<int, double>> observedActivations;
  long long disjoint = 0;
  double endpoint = 0, nativeEnd = 0;
  double bulkBefore = 0, bulkAfter = 0;
  std::map<std::string, double> bulkInitial, bulkFinal;
  G4DNAScavengerMaterial* bath = nullptr;
  double Bulk() {
    auto* current = dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial());
    if (!current || (bath && bath != current)) throw std::runtime_error("bulk material changed");
    bath = current;
    auto* oxygen = G4MolecularConfiguration::GetMolecularConfiguration("O2");
    if (!oxygen) throw std::runtime_error("oxygen configuration unavailable");
    return current->GetNumberMoleculePerVolumeUnitForMaterialConf(oxygen);
  }
  std::map<std::string, double> BulkSnapshot() {
    Bulk();
    std::map<std::string, double> result;
    for (const auto* species : bath->GetScavengerList())
      result[species->GetName()] = bath->GetNumberMoleculePerVolumeUnitForMaterialConf(species);
    return result;
  }
  void StartProcessing() override { bulkBefore = Bulk(); bulkInitial = BulkSnapshot(); }
  void NewStage() override {
    std::cout << "BULK_OBSERVED " << G4Scheduler::Instance()->GetGlobalTime() / ps << ' ' << Bulk() << std::endl;
    for (auto* track : *G4ITTrackHolder::Instance()->GetMainList()) {
      if (pending.erase(track->GetTrackID())) {
        if (track->GetGlobalTime() != G4Scheduler::Instance()->GetGlobalTime())
          throw std::runtime_error("source registered outside activation");
        G4MoleculeCounterManager::Instance()->AddMolecule(track, track->GetGlobalTime());
        observedActivations.emplace_back(track->GetTrackID(), G4Scheduler::Instance()->GetGlobalTime());
        std::cout << "OBSERVED_ACTIVATION " << track->GetTrackID() << ' ' << G4Scheduler::Instance()->GetGlobalTime() / ps << std::endl;
        ++registered;
      }
    }
  }
  void UserReactionAction(const G4Track& a, const G4Track& b,
                          const std::vector<G4Track*>* products) override {
    const unsigned x = Mask(a), y = Mask(b);
    if (x && y && !(x & y)) ++disjoint;
    if (products) for (auto* product : *products) pendingMasks[product] = x | y;
  }
  void EndProcessing() override {
    HistoryAction::EndProcessing();
    nativeEnd = G4Scheduler::Instance()->GetGlobalTime();
    for (auto* track : *G4ITTrackHolder::Instance()->GetMainList()) {
      if (track->GetTrackStatus() == fStopAndKill) continue;
      const double tolerance = 64 * std::numeric_limits<double>::epsilon() * std::max(endpoint, 1 * ps);
      if (std::abs(track->GetGlobalTime() - endpoint) > tolerance)
        throw std::runtime_error("live track did not reach observation endpoint");
    }
    bulkAfter = Bulk(); bulkFinal = BulkSnapshot();
    if (!pending.empty()) throw std::runtime_error("source activation incomplete");
    const auto counters = G4MoleculeCounterManager::Instance()->GetMoleculeCounters("BasicCounter");
    if (counters.size() != 1) throw std::runtime_error("expected one BasicCounter");
    const auto* counter = dynamic_cast<const G4MoleculeCounter*>(counters.front());
    if (!counter) throw std::runtime_error("unexpected counter type");
    std::set<std::string> names;
    for (const auto* molecule : counter->GetRecordedMolecules()) {
      names.insert(molecule->GetName());
      if (counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(molecule), G4Scheduler::Instance()->GetEndTime()) != yields[molecule->GetName()])
        throw std::runtime_error("counter/live mismatch: " + molecule->GetName());
    }
    for (auto* track : *G4ITTrackHolder::Instance()->GetMainList())
      if (track->GetTrackStatus() != fStopAndKill && GetMolecule(track)->GetDefinition() != G4H2O::Definition() && !names.count(GetMolecule(track)->GetName()))
        throw std::runtime_error("live species missing from counter");
  }
};
struct CounterStagingGuard {
  G4MoleculeCounterManager* manager = G4MoleculeCounterManager::Instance();
  bool active = manager->GetIsActive();
  CounterStagingGuard() { if (!active) throw std::runtime_error("counter inactive"); manager->SetIsActive(false); }
  ~CounterStagingGuard() { manager->SetIsActive(active); }
};
}

// manifest: one quoted packet path per line. Fixed schedule: arrival_ps packet_index.
// mode argument is either a schedule filename or a numeric dose rate when mode is poisson.
int main(int argc, char** argv) {
 try {
  if (argc != 12) throw std::runtime_error("macro manifest output exposure_ps endpoint_ps chemistry_seed arrival_seed selection_seed fixed|poisson mode_argument max_sources");
  const double exposurePs = std::stod(argv[4]), endpointPs = std::stod(argv[5]);
  const long chemistrySeed = std::stol(argv[6]);
  const auto arrivalSeed = std::stoull(argv[7]), selectionSeed = std::stoull(argv[8]);
  const std::string mode = argv[9]; const int maxSources = std::stoi(argv[11]);
  if (!std::isfinite(exposurePs) || !std::isfinite(endpointPs) || exposurePs <= 0 || endpointPs <= exposurePs || endpointPs > 3e9 || maxSources < 1 || maxSources > 8 || chemistrySeed <= 0)
    throw std::runtime_error("invalid bounded window or seed");
  const auto started = std::chrono::steady_clock::now();
  SourceContacts::enabled = true; SourceContacts::ResetPrivateHistory();
  auto manager = std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;
  manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); if (!macro) throw std::runtime_error("missing macro");
  std::string line, identity;
  while (std::getline(macro, line)) { identity += line + "\n"; if (!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed"); }
  std::ifstream manifest(argv[2]); if (!manifest) throw std::runtime_error("missing manifest");
  std::vector<Packets::Packet> library; std::vector<std::string> paths; std::string path;
  while (std::getline(manifest, line)) {
    std::istringstream row(line); row >> std::ws; if (row.eof()) continue;
    if (!(row >> std::quoted(path))) throw std::runtime_error("bad manifest"); row >> std::ws;
    if (!row.eof()) throw std::runtime_error("bad manifest record");
    std::ifstream in(path); if (!in) throw std::runtime_error("missing packet: " + path);
    library.push_back(Packets::Read(in, std::string(PACKET_COMPATIBILITY) + "|" + identity)); paths.push_back(path);
    if (library.size() > 16) throw std::runtime_error("library exceeds sixteen-packet bound");
  }
  if (library.empty()) throw std::runtime_error("empty packet library");
  auto* world = G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  const double mass = world->GetMaterial()->GetDensity() * world->GetSolid()->GetCubicVolume() / kg;
  double meanEnergy = 0;
  for (const auto& packet : library) {
    if (packet.massKg != mass || packet.halfSide != 2*um) throw std::runtime_error("packet geometry mismatch");
    meanEnergy += packet.energyDeposit / library.size();
    for (const auto& molecule : packet.molecules) for (double x : molecule.position)
      if (std::abs(x) > packet.halfSide) throw std::runtime_error("source position outside world");
  }
  double meanEnergySE = 0;
  if (library.size() > 1) {
    for (const auto& packet : library) meanEnergySE += (packet.energyDeposit - meanEnergy) * (packet.energyDeposit - meanEnergy);
    meanEnergySE = std::sqrt(meanEnergySE / (library.size() * (library.size() - 1)));
  }
  std::vector<BeamSchedule::Arrival> schedule;
  double rate = 0, doseRate = 0;
  if (mode == "fixed") {
    std::ifstream input(argv[10]); if (!input) throw std::runtime_error("missing schedule");
    schedule = FiniteSchedule::Read(input, exposurePs, library.size());
  } else if (mode == "poisson") {
    doseRate = std::stod(argv[10]); rate = BeamSchedule::Rate(doseRate, mass, meanEnergy);
    std::mt19937_64 arrivals(arrivalSeed), selection(selectionSeed);
    schedule = BeamSchedule::SampleBounded(rate, exposurePs * ps / second, library.size(), arrivals, selection, maxSources);
    for (auto& item : schedule) item.time *= second / ps;
  } else throw std::runtime_error("unknown schedule mode");
  if (schedule.size() > static_cast<std::size_t>(maxSources)) throw std::runtime_error("source cap exceeded; history censored");
  const auto initialized = std::chrono::steady_clock::now();
  auto* action = new BeamAction; action->endpoint = endpointPs * ps; G4Scheduler::Instance()->SetUserAction(action);
  int id = 1000000; std::size_t sourceCount = 0; double energy = 0;
  std::vector<std::tuple<int, std::size_t, double>> expectedActivations;
  {
    CounterStagingGuard guard;
    for (std::size_t k = 0; k < schedule.size(); ++k) {
      const auto& entry = schedule[k]; const auto& packet = library.at(entry.packet); energy += packet.energyDeposit;
      for (const auto& molecule : packet.molecules) {
        const double activation = entry.time * ps + molecule.globalTime;
        if (!std::isfinite(activation) || activation >= endpointPs * ps) throw std::runtime_error("endpoint excludes activation");
        auto* track = (new G4Molecule(ResolveHistory(molecule)))->BuildTrack(activation, {molecule.position[0], molecule.position[1], molecule.position[2]});
        track->SetTrackID(++id); track->SetTrackStatus(static_cast<G4TrackStatus>(molecule.status));
        track->SetMomentumDirection({molecule.direction[0], molecule.direction[1], molecule.direction[2]});
        track->SetKineticEnergy(molecule.kineticEnergy); track->SetLocalTime(molecule.localTime); track->SetProperTime(molecule.properTime);
        SourceContacts::activation[id] = activation; SourceContacts::RegisterPrivateSource(id, activation);
        expectedActivations.emplace_back(id, k, activation);
        packetMasks[id] = 1u << k; action->pending.insert(id); G4ITTrackHolder::Instance()->Push(track); ++sourceCount;
      }
    }
  }
  G4Scheduler::Instance()->SetEndTime(endpointPs * ps); G4Random::setTheSeed(chemistrySeed);
  G4DNAChemistryManager::Instance()->UseAsStandalone(true);
  std::ofstream out(argv[3]); out << std::setprecision(17);
  out << "mode " << mode << "\nexposure_ps " << exposurePs << "\nendpoint_ps " << endpointPs << "\nchemistry_seed " << chemistrySeed << "\narrival_seed " << arrivalSeed << "\nselection_seed " << selectionSeed << "\nrate_s_inv " << rate << "\ndose_rate_Gy_s " << doseRate << "\nmean_packet_energy_MeV " << meanEnergy << "\nmean_packet_energy_se_MeV " << meanEnergySE << "\nmass_kg " << mass << "\npackets " << schedule.size() << "\nsource_count " << sourceCount << "\nenergy_at_beam_end_MeV " << energy << "\ndose_at_beam_end_Gy " << BeamSchedule::Dose(energy, mass) << '\n';
  std::size_t pendingAtBeamEnd = 0;
  for (std::size_t k = 0; k < schedule.size(); ++k) {
    const auto& a = schedule[k]; const auto& p = library[a.packet];
    out << "arrival " << k << " time_ps " << a.time << " packet " << a.packet << " path " << std::quoted(paths[a.packet]) << " energy_MeV " << p.energyDeposit << " molecules " << p.molecules.size() << '\n';
  }
  for (const auto& [trackID, source, time] : expectedActivations) {
    if (time / ps >= exposurePs) ++pendingAtBeamEnd;
    out << "expected_activation " << trackID << " source " << source << " time_ps " << time / ps << '\n';
  }
  out << "pending_at_beam_end " << pendingAtBeamEnd << '\n'; out.flush();
  if (!out) throw std::runtime_error("source ledger write failed");
  const auto chemistryStart = std::chrono::steady_clock::now();
  if (sourceCount) G4DNAChemistryManager::Instance()->Run();
  else { action->bulkBefore = action->Bulk(); action->bulkAfter = action->bulkBefore; action->bulkInitial = action->BulkSnapshot(); action->bulkFinal = action->bulkInitial; }
  const auto chemistryEnd = std::chrono::steady_clock::now();
  if (premature || action->registered != sourceCount || !action->pending.empty()) throw std::runtime_error("activation budget failed");
  out << "registrations " << action->registered << "\npending_at_final " << action->pending.size() << "\ndisjoint " << action->disjoint << '\n';
  for (const auto& [trackID, time] : action->observedActivations)
    out << "observed_activation " << trackID << " time_ps " << time / ps << '\n';
  out << "bulk_before " << action->bulkBefore << "\nbulk_after " << action->bulkAfter << '\n';
  for (const auto& [name, count] : action->bulkInitial) out << "bulk_initial " << std::quoted(name) << ' ' << count << '\n';
  for (const auto& [name, count] : action->bulkFinal) out << "bulk_final " << std::quoted(name) << ' ' << count << '\n';
  if (!sourceCount) for (int i = 0; i < G4MolecularConfiguration::GetNumberOfSpecies(); ++i)
    yields[G4MolecularConfiguration::GetMolecularConfiguration(i)->GetName()] = 0;
  long long totalLive = 0; for (const auto& [name, count] : yields) totalLive += count;
  out << "observation_ps " << endpointPs << " live " << totalLive << "\nnative_end_ps " << (sourceCount ? action->nativeEnd / ps : 0) << '\n';
  for (const auto& [name, count] : yields) out << "species " << std::quoted(name) << ' ' << count << '\n';
  out << "init_seconds " << std::chrono::duration<double>(initialized - started).count() << "\nchemistry_seconds " << std::chrono::duration<double>(chemistryEnd - chemistryStart).count() << '\n';
  if (!out) throw std::runtime_error("output failed");
  std::cout << "BEAM_ON_HISTORY_PASS\n"; return 0;
 } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
