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
    std::set<std::string> recordedNames;
    for(const auto* molecule:counter->GetRecordedMolecules()) {
      recordedNames.insert(molecule->GetName());
      const auto count=counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(molecule),G4Scheduler::Instance()->GetEndTime());
      if(count!=yields[molecule->GetName()])
        throw std::runtime_error("final counter/live-population mismatch for "+molecule->GetName());
    }
    for(const auto& [name,count]:yields) if(count && !recordedNames.count(name)) throw std::runtime_error("live species absent from BasicCounter: "+name);
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

class BoundaryAction : public ScheduledAction {
public:
 bool sawOldBeforeBoundary=false, sawBoundary=false, spectatorAlive=false; int spectatorId=0;
 void EndProcessing() override {
  ScheduledAction::EndProcessing();
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) if(t->GetTrackID()==spectatorId && t->GetTrackStatus()!=fStopAndKill) spectatorAlive=true;
 }
 void UserPreTimeStepAction() override {
  HistoryAction::UserPreTimeStepAction();
  if(G4Scheduler::Instance()->GetGlobalTime()<1.5*ps && G4ITTrackHolder::Instance()->GetMainList()->begin()!=G4ITTrackHolder::Instance()->GetMainList()->end()) sawOldBeforeBoundary=true;
 }
 void NewStage() override {
  if(G4Scheduler::Instance()->GetGlobalTime()==1.5*ps) sawBoundary=true;
  ScheduledAction::NewStage();
 }
};
int main(int argc,char** argv) {
 try {
  if(argc!=6) throw std::runtime_error("macro packet output seed spectator");
  const bool spectator=std::stoi(argv[5])!=0;
  SourceContacts::enabled=true; SourceContacts::ResetPrivateHistory();
  std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); std::string line,identity; if(!macro) throw std::runtime_error("missing macro");
  while(std::getline(macro,line)) { identity+=line+"\n"; if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed"); }
  std::ifstream in(argv[2]); auto packet=Packets::Read(in,std::string(PACKET_COMPATIBILITY)+"|"+identity);
  auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  if(world->GetMaterial()->GetDensity()*world->GetSolid()->GetCubicVolume()/kg!=packet.massKg) throw std::runtime_error("mass mismatch");
  auto* action=new BoundaryAction; G4Scheduler::Instance()->SetUserAction(action);
  int id=1000000; double maxTime=0; double minDistance=1e99;
  {
   CounterStagingGuard guard;
   for(const auto& m:packet.molecules) {
    maxTime=std::max(maxTime,m.globalTime); G4ThreeVector pos(m.position[0],m.position[1],m.position[2]);
    minDistance=std::min(minDistance,(pos-G4ThreeVector(100*um,100*um,100*um)).mag());
    auto* t=(new G4Molecule(ResolveHistory(m)))->BuildTrack(m.globalTime,pos); t->SetTrackID(++id); t->SetTrackStatus(static_cast<G4TrackStatus>(m.status)); t->SetMomentumDirection({m.direction[0],m.direction[1],m.direction[2]}); t->SetKineticEnergy(m.kineticEnergy); t->SetLocalTime(m.localTime); t->SetProperTime(m.properTime);
    SourceContacts::activation[id]=m.globalTime; SourceContacts::RegisterPrivateSource(id,m.globalTime); packetMasks[id]=1; action->pending.insert(id); G4ITTrackHolder::Instance()->Push(t);
   }
   if(maxTime>=1.5*ps || minDistance<10*um) throw std::runtime_error("packet does not satisfy time/distance separation");
   if(spectator) {
    auto* config=G4MolecularConfiguration::GetMolecularConfiguration("O2"); if(!config) throw std::runtime_error("missing O2");
    auto* t=(new G4Molecule(config))->BuildTrack(1.5*ps,{100*um,100*um,100*um}); t->SetTrackID(++id);
    action->spectatorId=id; SourceContacts::activation[id]=1.5*ps; SourceContacts::RegisterPrivateSource(id,1.5*ps); packetMasks[id]=2; action->pending.insert(id); G4ITTrackHolder::Instance()->Push(t);
   }
  }
  G4Scheduler::Instance()->SetEndTime(3*ps); G4Random::setTheSeed(std::stol(argv[4])); G4DNAChemistryManager::Instance()->UseAsStandalone(true); G4DNAChemistryManager::Instance()->Run();
  if(premature||!action->sawOldBeforeBoundary||action->registered!=packet.molecules.size()+spectator||cross || (spectator&&!action->sawBoundary)) throw std::runtime_error("boundary or ancestry invariant failed");
  if(spectator) { if(!action->spectatorAlive) throw std::runtime_error("spectator did not survive"); --yields["O_2^0"]; }
  std::ofstream out(argv[3]); out<<std::setprecision(17)<<"energy_MeV "<<packet.energyDeposit<<"\nmass_kg "<<packet.massKg<<"\nsource_count "<<packet.molecules.size()<<"\nmax_source_time_ps "<<maxTime/ps<<"\nminimum_distance_um "<<minDistance/um<<"\nregistrations "<<action->registered<<"\nboundary "<<action->sawBoundary<<"\ncross "<<cross<<'\n';
  for(const auto& [name,count]:yields) out<<std::quoted(name)<<' '<<count<<'\n'; if(!out) throw std::runtime_error("output failed"); std::cout<<"BOUNDARY_PASS\n"; return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 2;}
}



