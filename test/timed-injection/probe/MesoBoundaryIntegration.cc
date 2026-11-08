#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "G4ChemReboundTransportation.hh"
#include "BoundedBrownianAction.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeCounterManager.hh"
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
class EdgeAction final : public G4UserTimeStepAction {
public:
 G4DNABoundingBox* box=nullptr; long survivors=0;
 void EndProcessing() override {
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) if(t->GetTrackStatus()!=fStopAndKill) {
   auto* conf=GetMolecule(t)->GetMolecularConfiguration();
   if(t->GetTrackStatus()!=fAlive || conf->GetDefinition()->GetParticleName()=="H2O" || conf->GetDecayTime()>=0) throw std::runtime_error("unresolved edge precursor");
   if(!box->contains(t->GetPosition())) throw std::runtime_error("edge survivor outside");
   ++survivors;
  }
 }
};
int main(int argc,char** argv) {
 try {
  if(argc!=6) throw std::runtime_error("macro transport|dissociation axis side seed");
  std::string mode=argv[2]; int axis=std::stoi(argv[3]),side=std::stoi(argv[4]);
  if((mode!="transport" && mode!="dissociation")||axis<0||axis>2||(side!=-1 && side!=1)) throw std::runtime_error("invalid fixture");
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
  std::ifstream in(argv[1]);std::string line;while(std::getline(in,line)) if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
#ifdef _WIN32
  HMODULE modules[2048];DWORD bytes=0;if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules)) throw std::runtime_error("module inventory failed");
  for(DWORD i=0;i<bytes/sizeof(HMODULE);++i){char path[MAX_PATH];if(!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH)) throw std::runtime_error("module path failed");std::cout<<"MODULE "<<path<<"\n";}
#endif
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  G4DNABoundingBox box({2*um,-2*um,2*um,-2*um,2*um,-2*um});
  const G4MolecularConfiguration* conf=nullptr;
  if(mode=="transport") conf=G4MolecularConfiguration::GetMolecularConfiguration("H2");
  else for(int i=0;i<G4MolecularConfiguration::GetNumberOfSpecies();++i){auto* c=G4MolecularConfiguration::GetMolecularConfiguration(i);if(c->GetDefinition()->GetParticleName()=="H2O" && c->GetCharge()==1){conf=c;break;}}
  if(!conf) throw std::runtime_error("missing edge molecule");
  G4ThreeVector p;p[axis]=side*(2*um-0.001*nm);auto* t=(new G4Molecule(conf))->BuildTrack(1*ps,p);t->SetTrackID(1000001);if(mode=="dissociation")t->SetTrackStatus(fStopButAlive);G4ITTrackHolder::Instance()->Push(t);
  auto* action=new EdgeAction;action->box=&box;G4Scheduler::Instance()->SetUserAction(action);G4Scheduler::Instance()->SetEndTime(100*ps);
  G4ChemReboundTransportation::ProbeResetBoundaryCount();BoundedBrownianAction::ProbeResetBoundaryCount();G4Random::setTheSeed(std::stol(argv[5]));G4DNAChemistryManager::Instance()->UseAsStandalone(true);G4DNAChemistryManager::Instance()->Run();
  if(action->survivors<1) throw std::runtime_error("no edge survivor");
  std::cout<<"EDGE_CROSS "<<G4ChemReboundTransportation::ProbeBoundaryCount()<<" "<<BoundedBrownianAction::ProbeBoundaryCount()<<" "<<BoundedBrownianAction::ProbeCallCount()<<"\nEDGE_PASS\n";return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
