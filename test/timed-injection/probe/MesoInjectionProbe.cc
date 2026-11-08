#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include <random>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
struct Survivor { const G4MolecularConfiguration* config; G4ThreeVector position; };
class CaptureFinal final : public G4UserTimeStepAction {
public:
 std::vector<Survivor> survivors;
 double endpoint=0;
 void EndProcessing() override {
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) {
   if(t->GetTrackStatus()==fStopAndKill) continue;
   const auto* config=GetMolecule(t)->GetMolecularConfiguration();
   std::cout << "SURVIVOR_STATE " << std::quoted(config->GetName()) << " definition " << config->GetDefinition()->GetParticleName() << " status " << t->GetTrackStatus() << " decay " << config->GetDecayTime() << "\n";
   if(t->GetTrackStatus()!=fAlive || config->GetDefinition()->GetParticleName()=="H2O" || config->GetDecayTime()>=0) throw std::runtime_error("unresolved precursor or decay at transfer");
   if(std::abs(t->GetGlobalTime()-endpoint)>1e-9*ps) throw std::runtime_error("particle clock mismatch");
   survivors.push_back({GetMolecule(t)->GetMolecularConfiguration(),t->GetPosition()});
  }
 }
};
int main(int argc,char** argv) {
 try {
  if(argc!=6) throw std::runtime_error("macro mode seed arrival_ps stable|precursor");
  const bool precursor=std::string(argv[5])=="precursor";
  if(!precursor && std::string(argv[5])!="stable") throw std::runtime_error("unknown old source");
  const double arrival=std::stod(argv[4])*ps;
  if(arrival<1*ps || arrival>40000*ps) throw std::runtime_error("arrival outside bounded fixture");
  const double transfer=arrival+2*ps, endpoint=arrival+3*ps;
  const double firstHandover=precursor?100*ps:1*ps;
  if(arrival<=firstHandover) throw std::runtime_error("arrival must follow first handover");
  const bool cycle=std::string(argv[2])=="cycle";
  if(!cycle && std::string(argv[2])!="particle") throw std::runtime_error("invalid mode");
  SourceContacts::enabled=true; SourceContacts::ResetPrivateHistory();
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); std::string line;
  while(std::getline(macro,line)) if(!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
#ifdef _WIN32
  HMODULE modules[2048]; DWORD bytes=0;
  if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes) || bytes>sizeof(modules)) throw std::runtime_error("module inventory failed");
  for(DWORD i=0;i<bytes/sizeof(HMODULE);++i) { char path[MAX_PATH]; if(!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH)) throw std::runtime_error("module path failed"); std::cout << "MODULE " << path << "\n"; }
#endif
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  auto* bath=G4Scheduler::Instance()->GetScavengerMaterial();
  G4Scheduler::Instance()->ResetScavenger(false);
  G4DNABoundingBox box({2*um,-2*um,2*um,-2*um,2*um,-2*um});
  auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  if(std::abs(world->GetSolid()->GetCubicVolume()/box.Volume()-1)>1e-12) throw std::runtime_error("mesh and bulk/world volumes differ");
  auto* oh=G4MolecularConfiguration::GetMolecularConfiguration("OH");
  if(!oh) throw std::runtime_error("missing OH");
  const G4MolecularConfiguration* hole=nullptr;
  for(int i=0;i<G4MolecularConfiguration::GetNumberOfSpecies();++i) {auto* c=G4MolecularConfiguration::GetMolecularConfiguration(i); if(c->GetDefinition()->GetParticleName()=="H2O" && c->GetCharge()==1){hole=c;break;}}
  if(!hole) throw std::runtime_error("missing water hole");
  G4DNAEventScheduler meso; meso.SetStartTime(firstHandover); meso.SetChangeMesh(false);
  G4Random::setTheSeed(std::stol(argv[3]));
  G4DNAChemistryManager::Instance()->UseAsStandalone(true);
  G4ThreeVector oldPosition(0.01*nm,0.01*nm,0.01*nm);
  std::vector<Survivor> initialSurvivors;
  if(precursor) {
   auto* oldIon=(new G4Molecule(hole))->BuildTrack(1*ps,oldPosition); oldIon->SetTrackID(1000001); oldIon->SetTrackStatus(fStopButAlive);
   SourceContacts::activation[1000001]=1*ps; SourceContacts::RegisterPrivateSource(1000001,1*ps); G4ITTrackHolder::Instance()->Push(oldIon);
   auto* oldElectron=(new G4Molecule(G4MolecularConfiguration::GetMolecularConfiguration("e_aq")))->BuildTrack(1*ps,{0.11*nm,0,0}); oldElectron->SetTrackID(1000002);
   SourceContacts::activation[1000002]=1*ps; SourceContacts::RegisterPrivateSource(1000002,1*ps); G4ITTrackHolder::Instance()->Push(oldElectron);
   if(cycle) {
    auto* first=new CaptureFinal; first->endpoint=firstHandover; G4Scheduler::Instance()->SetUserAction(first); G4Scheduler::Instance()->SetEndTime(firstHandover);
    G4DNAChemistryManager::Instance()->Run(); initialSurvivors=first->survivors;
    G4Scheduler::Instance()->SetUserAction(nullptr); delete first;
    if(G4Scheduler::Instance()->GetScavengerMaterial()!=bath || G4ITTrackHolder::Instance()->DelayListsNOTEmpty()) throw std::runtime_error("first phase bulk or delayed-list mismatch");
    SourceContacts::ResetPrivateHistory(); SourceContacts::activation.clear();
   }
  } else if(!cycle) {
   auto* t=(new G4Molecule(oh))->BuildTrack(1*ps,oldPosition); t->SetTrackID(1000001); SourceContacts::activation[1000001]=1*ps; SourceContacts::RegisterPrivateSource(1000001,1*ps); G4ITTrackHolder::Instance()->Push(t);
  }
  if(cycle) {
   meso.Initialize(box,160);
   if(!G4DNAEventScheduler::CheckingReactionRadius(meso.GetMesh()->GetResolution())) throw std::runtime_error("invalid reaction resolution");
   auto* mesh=meso.GetMesh();
   if(precursor) {for(auto& item:initialSurvivors) ++mesh->GetVoxelMapList(mesh->GetIndex(item.position))[item.config];}
   else mesh->GetVoxelMapList(mesh->GetIndex(oldPosition))[oh]=1;
   meso.ProbeRefresh(); meso.ProbeAdvanceTo(arrival);
   std::mt19937_64 placement(std::stoull(argv[3])+981723); std::uniform_real_distribution<double> uniform(0.,1.);
   long count=0;
   for(auto it=mesh->begin();it!=mesh->end();++it) for(auto [conf,n]:std::get<2>(*it)) for(size_t j=0;j<n;++j) {
    auto cell=std::get<1>(*it); G4ThreeVector p(cell.Getxlo()+uniform(placement)*(cell.Getxhi()-cell.Getxlo()),cell.Getylo()+uniform(placement)*(cell.Getyhi()-cell.Getylo()),cell.Getzlo()+uniform(placement)*(cell.Getzhi()-cell.Getzlo()));
    auto* t=(new G4Molecule(conf))->BuildTrack(arrival,p); t->SetTrackID(3000000+ ++count); SourceContacts::activation[t->GetTrackID()]=arrival; SourceContacts::RegisterPrivateSource(t->GetTrackID(),arrival); G4ITTrackHolder::Instance()->Push(t);
   }
   if(count>1000 || meso.GetGlobalTime()!=arrival) throw std::runtime_error("old mesh budget/time mismatch");
   std::cout << "RECONSTRUCTED " << count << "\n";
   mesh->Reset();
  }
  auto* ion=(new G4Molecule(hole))->BuildTrack(arrival,{0,0,0}); ion->SetTrackID(2000001); ion->SetTrackStatus(fStopButAlive); SourceContacts::activation[2000001]=arrival; SourceContacts::RegisterPrivateSource(2000001,arrival); G4ITTrackHolder::Instance()->Push(ion);
  auto* electron=(new G4Molecule(G4MolecularConfiguration::GetMolecularConfiguration("e_aq")))->BuildTrack(arrival,{0.1*nm,0,0}); electron->SetTrackID(2000002); SourceContacts::activation[2000002]=arrival; SourceContacts::RegisterPrivateSource(2000002,arrival); G4ITTrackHolder::Instance()->Push(electron);
  auto* action=new CaptureFinal; G4Scheduler::Instance()->SetUserAction(action); action->endpoint=cycle?transfer:endpoint; G4Scheduler::Instance()->SetEndTime(action->endpoint);
  G4DNAChemistryManager::Instance()->UseAsStandalone(true); G4DNAChemistryManager::Instance()->Run();
  if(G4Scheduler::Instance()->GetScavengerMaterial()!=bath) throw std::runtime_error("bulk object changed");
  std::map<std::string,long> counts; for(auto& a:action->survivors) ++counts[a.config->GetName()];
  if(cycle) {
   auto* mesh=meso.GetMesh(); meso.ProbeRefresh(); meso.ProbeAdvanceTo(transfer);
   if(meso.GetGlobalTime()!=transfer) throw std::runtime_error("phase clocks not aligned");
   for(auto& a:action->survivors) ++mesh->GetVoxelMapList(mesh->GetIndex(a.position))[a.config];
   meso.ProbeRefresh(); meso.ProbeAdvanceTo(endpoint);
   long total=0; counts.clear(); for(auto it=mesh->begin();it!=mesh->end();++it) for(auto [c,n]:std::get<2>(*it)) {total+=n; counts[c->GetName()]+=n;}
   std::cout<<"MESH_FINAL_TOTAL "<<total<<"\n";
  }
  std::cout<<"PARTICLE_TRANSFER_TOTAL "<<action->survivors.size()<<"\n"; for(auto [n,c]:counts) std::cout<<"SPECIES "<<std::quoted(n)<<" "<<c<<"\n";
  std::cout<<"CYCLE_PROBE_COMPLETE\n"; return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}

