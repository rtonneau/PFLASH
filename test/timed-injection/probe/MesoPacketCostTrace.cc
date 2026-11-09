#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeCounterManager.hh"
#include <random>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
struct CycleSurvivor {const G4MolecularConfiguration* config; G4ThreeVector position;};
class CycleCapture final : public G4UserTimeStepAction {
public:
 double endpoint=0; std::vector<CycleSurvivor> survivors;
 void EndProcessing() override {
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) {
   if(t->GetTrackStatus()==fStopAndKill) continue;
   auto* config=GetMolecule(t)->GetMolecularConfiguration();
   if(t->GetTrackStatus()!=fAlive || config->GetDefinition()->GetParticleName()=="H2O" || config->GetDecayTime()>=0) throw std::runtime_error("unresolved precursor at handover");
   if(std::abs(t->GetGlobalTime()-endpoint)>1e-9*ps) throw std::runtime_error("handover clock mismatch");
   survivors.push_back({config,t->GetPosition()});
  }
 }
};
int main(int argc,char** argv) {
 try {
  if(argc!=8) throw std::runtime_error("macro packet1 packet2 output seed delay_ps copies");
  const auto programStart=std::chrono::steady_clock::now(); const int copies=std::stoi(argv[7]); if(copies<1||copies>4)throw std::runtime_error("copies outside bounded cost fixture");
  const double delay=std::stod(argv[6])*ps, first=100*ps, arrival=delay+1*ps, transfer=arrival+2*ps, endpoint=arrival+3*ps;
  if(delay<=99*ps||delay>40000*ps) throw std::runtime_error("delay outside bounded fixture");
  SourceContacts::enabled=true; SourceContacts::ResetPrivateHistory();
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); std::string line,identity; if(!macro) throw std::runtime_error("missing macro");
  while(std::getline(macro,line)){identity+=line+"\n"; if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");}
#ifdef _WIN32
  HMODULE modules[2048]; DWORD bytes=0; if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules)) throw std::runtime_error("module inventory failed");
  for(DWORD i=0;i<bytes/sizeof(HMODULE);++i){char path[MAX_PATH];if(!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH)) throw std::runtime_error("module path failed");std::cout<<"MODULE "<<path<<"\n";}
#endif
  std::vector<Packets::Packet> packets; for(int i=2;i<4;++i){std::ifstream in(argv[i]); packets.push_back(Packets::Read(in,std::string(PACKET_COMPATIBILITY)+"|"+identity));}
  auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  const double mass=world->GetMaterial()->GetDensity()*world->GetSolid()->GetCubicVolume()/kg;
  G4DNABoundingBox box({2*um,-2*um,2*um,-2*um,2*um,-2*um});
  if(std::abs(world->GetSolid()->GetCubicVolume()/box.Volume()-1)>1e-12) throw std::runtime_error("mesh and bulk/world volumes differ");
  for(auto& p:packets) if(p.massKg!=mass||p.halfSide!=2*um) throw std::runtime_error("packet geometry mismatch");
  G4MoleculeCounterManager::Instance()->SetIsActive(false); auto* bath=G4Scheduler::Instance()->GetScavengerMaterial(); G4Scheduler::Instance()->ResetScavenger(false);
  G4Random::setTheSeed(std::stol(argv[5])); G4DNAChemistryManager::Instance()->UseAsStandalone(true);
  PROCESS_MEMORY_COUNTERS baselineMemory{}; baselineMemory.cb=sizeof(baselineMemory); if(!GetProcessMemoryInfo(GetCurrentProcess(),&baselineMemory,sizeof(baselineMemory))) throw std::runtime_error("baseline memory failed");
  const auto initDone=std::chrono::steady_clock::now();
  auto phase=[](const char* name){PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));std::cerr<<"COST_PHASE "<<name<<" working_set="<<memory.WorkingSetSize<<" peak="<<memory.PeakWorkingSetSize<<"\n"<<std::flush;};
  auto pushPacket=[&](int which,double offset,int base){long count=0; for(int copy=0;copy<copies;++copy) for(auto& m:packets[which].molecules){double t=m.globalTime+offset; auto* track=(new G4Molecule(ResolveHistory(m)))->BuildTrack(t,{m.position[0],m.position[1],m.position[2]});track->SetTrackID(base+ ++count);track->SetTrackStatus(static_cast<G4TrackStatus>(m.status));track->SetMomentumDirection({m.direction[0],m.direction[1],m.direction[2]});track->SetKineticEnergy(m.kineticEnergy);track->SetLocalTime(m.localTime);track->SetProperTime(m.properTime);SourceContacts::activation[track->GetTrackID()]=t;SourceContacts::RegisterPrivateSource(track->GetTrackID(),t);G4ITTrackHolder::Instance()->Push(track);}return count;};
  phase("first-source-begin"); auto firstCount=pushPacket(0,0,1000000); phase("first-particle-begin"); auto* early=new CycleCapture; early->endpoint=first; G4Scheduler::Instance()->SetUserAction(early); G4Scheduler::Instance()->SetEndTime(first); G4DNAChemistryManager::Instance()->Run();
  phase("first-particle-end"); const auto firstDone=std::chrono::steady_clock::now(); auto old=early->survivors; G4Scheduler::Instance()->SetUserAction(nullptr);delete early;
  if(G4Scheduler::Instance()->GetScavengerMaterial()!=bath || G4ITTrackHolder::Instance()->DelayListsNOTEmpty()) throw std::runtime_error("first phase bulk or delayed-list mismatch");
  SourceContacts::ResetPrivateHistory(); SourceContacts::activation.clear();
  G4DNAEventScheduler meso; meso.SetStartTime(first); meso.SetChangeMesh(false); meso.Initialize(box,160); auto* mesh=meso.GetMesh();
  if(!G4DNAEventScheduler::CheckingReactionRadius(mesh->GetResolution())) throw std::runtime_error("invalid reaction resolution");
  for(auto& item:old) ++mesh->GetVoxelMapList(mesh->GetIndex(item.position))[item.config];
  phase("first-meso-begin"); meso.ProbeRefresh();meso.ProbeAdvanceTo(arrival); phase("first-meso-end");
  const auto mesoDone=std::chrono::steady_clock::now();
  std::mt19937_64 placement(std::stoull(argv[5])+981723);std::uniform_real_distribution<double> uniform(0.,1.); long reconstructed=0;
  for(auto it=mesh->begin();it!=mesh->end();++it) for(auto [conf,n]:std::get<2>(*it)) for(size_t j=0;j<n;++j){auto cell=std::get<1>(*it);G4ThreeVector pos(cell.Getxlo()+uniform(placement)*(cell.Getxhi()-cell.Getxlo()),cell.Getylo()+uniform(placement)*(cell.Getyhi()-cell.Getylo()),cell.Getzlo()+uniform(placement)*(cell.Getzhi()-cell.Getzlo()));auto* track=(new G4Molecule(conf))->BuildTrack(arrival,pos);track->SetTrackID(3000000+ ++reconstructed);SourceContacts::activation[track->GetTrackID()]=arrival;SourceContacts::RegisterPrivateSource(track->GetTrackID(),arrival);G4ITTrackHolder::Instance()->Push(track);}
  phase("reconstruction-end"); mesh->Reset(); auto secondCount=pushPacket(1,delay,4000000); phase("second-particle-begin"); auto* late=new CycleCapture;late->endpoint=transfer;G4Scheduler::Instance()->SetUserAction(late);G4Scheduler::Instance()->SetEndTime(transfer);G4DNAChemistryManager::Instance()->Run();
  phase("second-particle-end"); const auto secondDone=std::chrono::steady_clock::now();
  if(G4Scheduler::Instance()->GetScavengerMaterial()!=bath || G4ITTrackHolder::Instance()->DelayListsNOTEmpty()) throw std::runtime_error("second phase bulk or delayed-list mismatch");
  phase("final-meso-begin"); meso.ProbeRefresh();meso.ProbeAdvanceTo(transfer);for(auto& item:late->survivors) ++mesh->GetVoxelMapList(mesh->GetIndex(item.position))[item.config];meso.ProbeRefresh();meso.ProbeAdvanceTo(endpoint);
  phase("final-meso-end"); const auto finalDone=std::chrono::steady_clock::now(); PROCESS_MEMORY_COUNTERS peakMemory{}; peakMemory.cb=sizeof(peakMemory); if(!GetProcessMemoryInfo(GetCurrentProcess(),&peakMemory,sizeof(peakMemory)))throw std::runtime_error("peak memory failed");
  std::map<std::string,long> counts;for(auto it=mesh->begin();it!=mesh->end();++it) for(auto [conf,n]:std::get<2>(*it)) counts[conf->GetName()]+=n;
  std::ofstream out(argv[4]);out<<std::setprecision(17)<<"energy_MeV "<<copies*(packets[0].energyDeposit+packets[1].energyDeposit)<<"\nmass_kg "<<mass<<"\nsource_count "<<firstCount+secondCount<<"\nold_at_handover "<<old.size()<<"\nold_reconstructed "<<reconstructed<<"\n";
  auto seconds=[](auto a,auto b){return std::chrono::duration<double>(b-a).count();};
  out<<"init_seconds "<<seconds(programStart,initDone)<<"\nfirst_particle_seconds "<<seconds(initDone,firstDone)<<"\nfirst_meso_seconds "<<seconds(firstDone,mesoDone)<<"\nsecond_particle_seconds "<<seconds(mesoDone,secondDone)<<"\nfinal_meso_seconds "<<seconds(secondDone,finalDone)<<"\ncycle_seconds "<<seconds(initDone,finalDone)<<"\nbaseline_working_set_bytes "<<baselineMemory.WorkingSetSize<<"\npeak_working_set_bytes "<<peakMemory.PeakWorkingSetSize<<"\n";for(auto [name,n]:counts) out<<std::quoted(name)<<" "<<n<<"\n";if(!out) throw std::runtime_error("output failed");
  std::cout<<"PACKET_CYCLE_COMPLETE\n";return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
