#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "BeamSchedule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeCounter.hh"
#include <set>

namespace {
class ScheduledAction : public HistoryAction {
public:
  std::set<int> pending;
  std::size_t registered=0;
  std::vector<std::pair<double,std::map<std::string,long long>>> samples;
  void Sample() {
    std::map<std::string,long long> counts;
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList())
      if(track->GetTrackStatus()!=fStopAndKill) ++counts[GetMolecule(track)->GetName()];
    samples.emplace_back(G4Scheduler::Instance()->GetGlobalTime(),std::move(counts));
  }
  void NewStage() override {
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList()) {
      if(pending.erase(track->GetTrackID())) {
        if(track->GetGlobalTime()!=G4Scheduler::Instance()->GetGlobalTime())
          throw std::runtime_error("source counter registration is not at activation");
        G4MoleculeCounterManager::Instance()->AddMolecule(track,track->GetGlobalTime());
        ++registered;
      }
    }
    Sample();
  }
  void EndProcessing() override {
    HistoryAction::EndProcessing();
    if(!pending.empty()) throw std::runtime_error("source counters left unregistered");
    const auto counters=G4MoleculeCounterManager::Instance()->GetMoleculeCounters("BasicCounter");
    if(counters.size()!=1) throw std::runtime_error("expected one BasicCounter");
    const auto* counter=dynamic_cast<const G4MoleculeCounter*>(counters.front());
    if(!counter) throw std::runtime_error("unexpected counter type");
    for(const auto* molecule:counter->GetRecordedMolecules()) {
      const auto count=counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(molecule),G4Scheduler::Instance()->GetEndTime());
      if(count!=yields[molecule->GetName()])
        throw std::runtime_error("final counter/live-population mismatch for "+molecule->GetName());
    }
    Sample();
  }
};
struct CounterStagingGuard {
  G4MoleculeCounterManager* manager=G4MoleculeCounterManager::Instance();
  bool active=manager->GetIsActive();
  CounterStagingGuard() {
    if(!active) throw std::runtime_error("schedule validation requires active counters");
    manager->SetIsActive(false);
  }
  ~CounterStagingGuard() { manager->SetIsActive(active); }
};
}

int main(int argc,char** argv)
{
  try {
    if(argc!=8) throw std::runtime_error("macro library output seed doseRate_Gy_s exposure_ps observation_ps");
    const auto seed=std::stoull(argv[4]);
    const double doseRate=std::stod(argv[5]);
    const double exposure=std::stod(argv[6])*ps;
    const double endpoint=std::stod(argv[7])*ps;
    if(endpoint<=exposure || endpoint>1*ns) throw std::runtime_error("invalid bounded observation window");
    SourceContacts::enabled=true;
    SourceContacts::ResetPrivateHistory();
    const auto started=std::chrono::steady_clock::now();
    std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new DetectorConstruction);
    manager->SetUserInitialization(new PhysicsList);
    manager->SetUserInitialization(new ActionInitialization);
    std::ifstream macro(argv[1]);
    if(!macro) throw std::runtime_error("missing macro");
    std::string line,identity;
    while(std::getline(macro,line)) {
      identity+=line+"\n";
      if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
    }
    std::vector<Packets::Packet> library;
    double meanEnergy=0;
    for(int i=0;i<8;++i) {
      std::ifstream in(std::string(argv[2])+"/library"+std::to_string(i)+".packet");
      library.push_back(Packets::Read(in,std::string(PACKET_COMPATIBILITY)+"|"+identity));
      meanEnergy+=library.back().energyDeposit/8;
    }
    const double mass=library.front().massKg;
    auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
    if(world->GetMaterial()->GetDensity()*world->GetSolid()->GetCubicVolume()/kg!=mass) throw std::runtime_error("mass mismatch");
    const double rate=BeamSchedule::Rate(doseRate,mass,meanEnergy);
    std::mt19937_64 arrivalEngine(seed), selectionEngine(seed+1);
    const auto schedule=BeamSchedule::Sample(rate,exposure/second,library.size(),arrivalEngine,selectionEngine);
    if(schedule.size()>8) throw std::runtime_error("history exceeds eight-packet resource bound; not resampled");
    std::ofstream out(argv[3]);
    out<<std::setprecision(17);
    int id=1000000;
    std::size_t sourceCount=0;
    double depositedEnergy=0;
    auto* action=new ScheduledAction;
    G4Scheduler::Instance()->SetUserAction(action);
    {
    // Construction stages future tracks; counting occurs at their actual activation.
    CounterStagingGuard staging;
    for(std::size_t k=0;k<schedule.size();++k) {
      const auto& arrival=schedule[k];
      const auto& packet=library[arrival.packet];
      if(packet.massKg!=mass || arrival.time>=exposure/second) throw std::runtime_error("incompatible arrival");
      out<<"arrival "<<k<<" packet "<<arrival.packet<<" time_ps "<<arrival.time*second/ps<<'\n';
      depositedEnergy+=packet.energyDeposit;
      for(const auto& molecule:packet.molecules) {
        const double activation=arrival.time*second+molecule.globalTime;
        if(activation>=endpoint) throw std::runtime_error("observation endpoint excludes a source activation");
        auto* track=(new G4Molecule(ResolveHistory(molecule)))->BuildTrack(activation,{molecule.position[0],molecule.position[1],molecule.position[2]});
        track->SetTrackID(++id);
        track->SetTrackStatus(static_cast<G4TrackStatus>(molecule.status));
        track->SetMomentumDirection({molecule.direction[0],molecule.direction[1],molecule.direction[2]});
        track->SetKineticEnergy(molecule.kineticEnergy);
        track->SetLocalTime(molecule.localTime);
        track->SetProperTime(molecule.properTime);
        SourceContacts::activation[id]=activation;
        SourceContacts::RegisterPrivateSource(id,activation);
        packetMasks[id]=1u<<k;
        action->pending.insert(id);
        G4ITTrackHolder::Instance()->Push(track);
        ++sourceCount;
      }
    }
    }
    out.flush();
    G4Scheduler::Instance()->SetEndTime(endpoint);
    G4Random::setTheSeed(static_cast<long>(seed+2));
    G4DNAChemistryManager::Instance()->UseAsStandalone(true);
    if(!schedule.empty()) G4DNAChemistryManager::Instance()->Run();
    if(premature) throw std::runtime_error("premature activation");
    if(action->registered!=sourceCount) throw std::runtime_error("source counter registration budget mismatch");
    out<<"packets "<<schedule.size()<<"\nsource_count "<<sourceCount
       <<"\nenergy_MeV "<<depositedEnergy<<"\nmass_kg "<<mass
       <<"\ndose_Gy "<<BeamSchedule::Dose(depositedEnergy,mass)
       <<"\nrate_Gy_s "<<doseRate<<"\narrival_rate_s_inv "<<rate
       <<"\nnominal_dose_Gy "<<doseRate*exposure/second
       <<"\narrival_seed "<<seed<<"\nselection_seed "<<seed+1<<"\nchemistry_seed "<<seed+2
       <<"\nexposure_ps "<<exposure/ps<<"\nendpoint_ps "<<endpoint/ps
       <<"\ncross "<<cross<<'\n';
    out<<"source_counter_registrations "<<action->registered<<'\n';
    for(const auto& [name,count]:yields) out<<std::quoted(name)<<' '<<count<<'\n';
    for(const auto& [time,counts]:action->samples)
      for(const auto& [name,count]:counts)
        out<<"sample "<<time/ps<<" "<<std::quoted(name)<<" "<<count<<'\n';
    out<<"seconds "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<'\n';
    if(!out) throw std::runtime_error("output failed");
    std::cout<<"SCHEDULED_HISTORY_PASS\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    return 2;
  }
}
