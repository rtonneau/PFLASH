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
class StressAction final : public G4UserTimeStepAction {
public:
 long electrons=0;
 void EndProcessing() override {for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) if(t->GetTrackStatus()!=fStopAndKill && GetMolecule(t)->GetMolecularConfiguration()==G4MolecularConfiguration::GetMolecularConfiguration("e_aq")) ++electrons;}
};
int main(int argc,char** argv) {
 try {
  if(argc!=6) throw std::runtime_error("macro particle|cycle near|far seed old|fresh");
  const std::string single=argv[5]; if(single!="old"&&single!="fresh")throw std::runtime_error("invalid single-source arm"); const bool includeOld=single=="old", includeFresh=single=="fresh";
  std::string mode=argv[2], where=argv[3]; if((mode!="particle"&&mode!="cycle")||(where!="near"&&where!="far")) throw std::runtime_error("invalid mode");
  SourceContacts::enabled=true;SourceContacts::ResetPrivateHistory();
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]);std::string line;while(std::getline(macro,line)) if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
#ifdef _WIN32
  HMODULE modules[2048];DWORD bytes=0;if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))throw std::runtime_error("module inventory failed");
  for(DWORD i=0;i<bytes/sizeof(HMODULE);++i){char path[MAX_PATH];if(!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH))throw std::runtime_error("module path failed");std::cout<<"MODULE "<<path<<"\n";}
#endif
  G4MoleculeCounterManager::Instance()->SetIsActive(false); G4Scheduler::Instance()->ResetScavenger(false);G4Random::setTheSeed(std::stol(argv[4]));
  G4DNABoundingBox box({2*um,-2*um,2*um,-2*um,2*um,-2*um}); auto* electron=G4MolecularConfiguration::GetMolecularConfiguration("e_aq");if(!electron)throw std::runtime_error("missing electron");
  const double activation=1.001*ps; G4ThreeVector old(where=="near"?0.01*nm:24.99*nm,0.01*nm,0.01*nm);
  if(mode=="cycle") {
   G4DNAEventScheduler meso;meso.SetStartTime(1*ps);meso.SetChangeMesh(false);meso.Initialize(box,160);auto* mesh=meso.GetMesh();
   if(!G4DNAEventScheduler::CheckingReactionRadius(mesh->GetResolution()))throw std::runtime_error("invalid reaction resolution");
   if(includeOld) mesh->GetVoxelMapList(mesh->GetIndex(old))[electron]=1;meso.ProbeRefresh();meso.ProbeAdvanceTo(activation);
   std::mt19937_64 placement(std::stoull(argv[4])+981723);std::uniform_real_distribution<double> uniform(0.,1.); long n=0;
   for(auto it=mesh->begin();it!=mesh->end();++it)for(auto [conf,count]:std::get<2>(*it))for(size_t j=0;j<count;++j){auto cell=std::get<1>(*it);G4ThreeVector pos(cell.Getxlo()+uniform(placement)*(cell.Getxhi()-cell.Getxlo()),cell.Getylo()+uniform(placement)*(cell.Getyhi()-cell.Getylo()),cell.Getzlo()+uniform(placement)*(cell.Getzhi()-cell.Getzlo()));auto* t=(new G4Molecule(conf))->BuildTrack(activation,pos);t->SetTrackID(1000001+ n++);SourceContacts::activation[t->GetTrackID()]=activation;SourceContacts::RegisterPrivateSource(t->GetTrackID(),activation);G4ITTrackHolder::Instance()->Push(t);}
   if(n!=(includeOld?1:0))throw std::runtime_error("old electron mesh budget");
  } else if(includeOld) {auto* t=(new G4Molecule(electron))->BuildTrack(1*ps,old);t->SetTrackID(1000001);SourceContacts::activation[1000001]=1*ps;SourceContacts::RegisterPrivateSource(1000001,1*ps);G4ITTrackHolder::Instance()->Push(t);}
  if(includeFresh) {auto* fresh=(new G4Molecule(electron))->BuildTrack(activation,{0.02*nm,0.01*nm,0.01*nm});fresh->SetTrackID(2000001);SourceContacts::activation[2000001]=activation;SourceContacts::RegisterPrivateSource(2000001,activation);G4ITTrackHolder::Instance()->Push(fresh);}
  auto* action=new StressAction;G4Scheduler::Instance()->SetUserAction(action);G4Scheduler::Instance()->SetEndTime(3*ps);G4DNAChemistryManager::Instance()->UseAsStandalone(true);G4DNAChemistryManager::Instance()->Run();
  std::cout<<"ELECTRONS "<<action->electrons<<"\nSTRESS_COMPLETE\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
