#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "BeamSchedule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeCounter.hh"
#include <set>
#include "G4H2O.hh"
#include "G4DNAScavengerMaterial.hh"

namespace {
class ScheduledAction : public HistoryAction {
public:
  double endpoint = 0; std::vector<G4ThreeVector> positions; long unresolved=0; double bulkBefore=0, bulkAfter=0;
  void StartProcessing() override { auto* b=dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial()); if(!b) throw std::runtime_error("missing bulk"); bulkBefore=b->GetNumberMoleculePerVolumeUnitForMaterialConf(G4MoleculeTable::Instance()->GetConfiguration("O2")); }
  std::set<int> pending;
  std::size_t registered=0;
  long long disjoint=0, overlapping=0;
  void UserReactionAction(const G4Track& a,const G4Track& b,const std::vector<G4Track*>* products) override {
    const unsigned x=Mask(a), y=Mask(b);
    if(x && y && !(x & y)) ++disjoint;
    else if(x && y && (x | y)==3 && ((x & y)!=0)) ++overlapping;
    if(products) for(auto* p:*products) pendingMasks[p]=x|y;
  }
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
    if(std::abs(G4Scheduler::Instance()->GetGlobalTime()-endpoint)>1e-9*ps) throw std::runtime_error("endpoint mismatch");
    for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) {
      if(t->GetTrackStatus()==fStopAndKill) continue;
      if(std::abs(t->GetGlobalTime()-endpoint)>1e-9*ps) throw std::runtime_error("particle endpoint mismatch");
      positions.push_back(t->GetPosition());
      auto* c=GetMolecule(t)->GetMolecularConfiguration();
      if(t->GetTrackStatus()!=fAlive || c->GetDefinition()==G4H2O::Definition() || c->GetDecayTime()>=0) ++unresolved;
    }
    auto* b=dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial());
    bulkAfter=b->GetNumberMoleculePerVolumeUnitForMaterialConf(G4MoleculeTable::Instance()->GetConfiguration("O2"));
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
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList())
      if(track->GetTrackStatus()!=fStopAndKill && GetMolecule(track)->GetDefinition()!=G4H2O::Definition() && !recordedNames.count(GetMolecule(track)->GetName()))
        throw std::runtime_error("eligible live species absent from BasicCounter: "+GetMolecule(track)->GetName());
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


int main(int argc,char** argv) {
 try {
  if(argc!=9) throw std::runtime_error("macro packet1 packet2 output seed mode delay_ps endpoint_ps");
  const int mode=std::stoi(argv[6]); const double delay=std::stod(argv[7])*ps;
  if(mode<0||mode>2||!std::isfinite(delay)||delay<0||delay>1.5*ps) throw std::runtime_error("invalid mode or delay");
  const double endpoint=std::stod(argv[8])*ps; if(!std::isfinite(endpoint)||endpoint<3*ps || endpoint>150*ps) throw std::runtime_error("endpoint outside cap");
  SourceContacts::enabled=true; SourceContacts::ResetPrivateHistory();
  auto started=std::chrono::steady_clock::now();
  std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); std::string line,identity; if(!macro) throw std::runtime_error("missing macro");
  while(std::getline(macro,line)) { identity+=line+"\n"; if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed"); }
  std::vector<Packets::Packet> packets;
  for(int i=2;i<=3;++i) { std::ifstream in(argv[i]); packets.push_back(Packets::Read(in,std::string(PACKET_COMPATIBILITY)+"|"+identity)); }
  auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  double mass=world->GetMaterial()->GetDensity()*world->GetSolid()->GetCubicVolume()/kg;
  if(packets[0].massKg!=mass||packets[1].massKg!=mass) throw std::runtime_error("mass mismatch");
  auto* action=new ScheduledAction; action->endpoint=endpoint; G4Scheduler::Instance()->SetUserAction(action);
  int id=1000000; std::size_t sourceCount=0; double maxAge=0;
  {
   CounterStagingGuard guard;
   for(int k=0;k<2;++k) if(mode==0 || mode==k+1) for(const auto& m:packets[k].molecules) {
    maxAge=std::max(maxAge,m.globalTime); const double activation=m.globalTime+k*delay;
    if(activation>=endpoint) throw std::runtime_error("source past endpoint");
    auto* t=(new G4Molecule(ResolveHistory(m)))->BuildTrack(activation,{m.position[0],m.position[1],m.position[2]});
    t->SetTrackID(++id); t->SetTrackStatus(static_cast<G4TrackStatus>(m.status)); t->SetMomentumDirection({m.direction[0],m.direction[1],m.direction[2]}); t->SetKineticEnergy(m.kineticEnergy); t->SetLocalTime(m.localTime); t->SetProperTime(m.properTime);
    SourceContacts::activation[id]=activation; SourceContacts::RegisterPrivateSource(id,activation); packetMasks[id]=(1u<<k); action->pending.insert(id); G4ITTrackHolder::Instance()->Push(t); ++sourceCount;
   }
  }
  if(maxAge>1*ps) throw std::runtime_error("source exceeds packet age");
  G4Scheduler::Instance()->SetEndTime(endpoint); G4Random::setTheSeed(std::stol(argv[5])); G4DNAChemistryManager::Instance()->UseAsStandalone(true); G4DNAChemistryManager::Instance()->Run();
  if(premature||action->registered!=sourceCount) throw std::runtime_error("activation or source budget failed");
  std::ofstream out(argv[4]); out<<std::setprecision(17);
  const double energy=(mode!=2?packets[0].energyDeposit:0)+(mode!=1?packets[1].energyDeposit:0);
  out<<"energy_MeV "<<energy<<"\nmass_kg "<<mass<<"\nsource_count "<<sourceCount<<"\nregistrations "<<action->registered<<"\ndose_Gy "<<BeamSchedule::Dose(energy,mass)<<"\ndisjoint "<<action->disjoint<<"\noverlapping "<<action->overlapping<<"\n";
  for(const auto& [name,count]:yields) out<<std::quoted(name)<<' '<<count<<'\n';
  out<<"total "<<action->positions.size()<<"\nunresolved "<<action->unresolved<<"\nbulk_before "<<action->bulkBefore<<"\nbulk_after "<<action->bulkAfter<<"\nendpoint_ps "<<endpoint/ps<<"\ntransferred 0\n";
  double minimum=std::numeric_limits<double>::infinity();
  for(size_t i=0;i<action->positions.size();++i) for(size_t j=i+1;j<action->positions.size();++j) minimum=std::min(minimum,(action->positions[i]-action->positions[j]).mag()/nm);
  out<<"minimum_separation_nm "<<minimum<<"\n";
  for(double cell:{20.,25.,50.}) for(double halo:{20.,40.,80.}) {
    long retained=0; const double distance=(halo+std::sqrt(3.)*cell/2)*nm;
    for(size_t i=0;i<action->positions.size();++i) {
      bool near=false;
      for(size_t j=0;j<action->positions.size();++j) if(i!=j && (action->positions[i]-action->positions[j]).mag()<=distance) {near=true;break;}
      retained+=near;
    }
    out<<"retained_"<<int(cell)<<"_"<<int(halo)<<' '<<retained<<"\n";
  }
  std::ofstream spatial(std::string(argv[4])+".positions"); spatial<<std::setprecision(17);
  for(const auto& p:action->positions) spatial<<p.x()/nm<<' '<<p.y()/nm<<' '<<p.z()/nm<<'\n';
  if(!spatial) throw std::runtime_error("snapshot write failed");
  out.close(); std::ofstream score(std::string(argv[4])+".score"); score<<std::setprecision(17)<<"pre_primary_0_minus 0 0\n";
  for(int k=0;k<2;++k) if(mode==0 || mode==k+1) score<<"packet "<<k<<" arrival_ps "<<k*delay/ps<<" energy_MeV "<<packets[k].energyDeposit<<" count "<<packets[k].molecules.size()<<"\n";
  double previous=0;
  for(const auto& [time,counts]:action->samples) {
   double sum=(mode!=2?packets[0].energyDeposit:0)+(mode!=1 && time>=delay?packets[1].energyDeposit:0); if(time<0) sum=0;
   if(sum<previous||sum>energy) throw std::runtime_error("dose ledger nonmonotonic"); previous=sum;
   score<<"sample "<<time/ps<<" energy_MeV "<<sum<<" dose_Gy "<<BeamSchedule::Dose(sum,mass)<<"\n";
   for(const auto& [name,count]:counts) score<<"population "<<time/ps<<' '<<std::quoted(name)<<' '<<count<<'\n';
  }
  if(previous!=energy) throw std::runtime_error("final dose mismatch");
  score<<"seconds "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<"\n";
  if(!out||!score) throw std::runtime_error("output failed"); std::cout<<"RETENTION_GATE_PASS\n"; return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 2;}
}


