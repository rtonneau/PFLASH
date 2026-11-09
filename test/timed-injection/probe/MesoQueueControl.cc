#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeTable.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4Scheduler.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "actions/ActionInitialization.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "Randomize.hh"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
int main(int argc,char** argv) {
 try {
  if(argc!=4) throw std::runtime_error("macro hook|native seed");
  const std::string mode=argv[2]; if(mode!="hook" && mode!="native") throw std::runtime_error("invalid mode");
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats; manager->SetUserInitialization(new DetectorConstruction); manager->SetUserInitialization(new PhysicsList); manager->SetUserInitialization(new ActionInitialization);
  std::ifstream in(argv[1]); std::string line; while(std::getline(in,line)) if(!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
#ifdef _WIN32
  HMODULE modules[2048]; DWORD bytes=0;
  if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes) || bytes>sizeof(modules)) throw std::runtime_error("module inventory failed");
  for(DWORD i=0;i<bytes/sizeof(HMODULE);++i) {char path[MAX_PATH]; if(!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH)) throw std::runtime_error("module path failed"); std::cout<<"MODULE "<<path<<"\n";}
#endif
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  auto bulkCounts=[](){std::map<const G4MolecularConfiguration*,G4double> out; auto* b=dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial()); if(b) for(auto* c:b->GetScavengerList()) out[c]=b->GetNumberMoleculePerVolumeUnitForMaterialConf(c); return out;};
  const auto initialBulk=bulkCounts();
  G4DNABoundingBox box({2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um});
  G4DNAEventScheduler scheduler; scheduler.SetStartTime(CLHEP::ps); scheduler.SetChangeMesh(false); scheduler.Initialize(box,160);
  auto* mesh=scheduler.GetMesh(); auto* species=G4MoleculeTable::Instance()->GetConfiguration("OH"); if(!species) throw std::runtime_error("OH missing");
  mesh->GetVoxelMapList(mesh->GetIndex({0.01*CLHEP::nm,0.01*CLHEP::nm,0.01*CLHEP::nm}))[species]=1;
  G4Random::setTheSeed(std::stol(argv[3])); scheduler.ProbeRefresh();
  const double end=40000*CLHEP::ps;
  if(mode=="hook") scheduler.ProbeAdvanceTo(end); else {scheduler.SetEndTime(end); scheduler.Run();}
  if(initialBulk!=bulkCounts()) throw std::runtime_error("bulk changed during no-change parity fixture");
  std::map<std::string,long> totals;
  for(auto it=mesh->begin();it!=mesh->end();++it) for(auto [conf,count]:std::get<2>(*it)) if(count){auto index=std::get<0>(*it); std::cout<<"CELL "<<index.x<<" "<<index.y<<" "<<index.z<<" "<<conf->GetName()<<" "<<count<<"\n"; totals[conf->GetName()]+=count;}
  for(auto [name,count]:totals) std::cout<<"TOTAL "<<name<<" "<<count<<"\n";
  std::cout<<"QUEUE_CONTROL_COMPLETE\n"; return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 2;}
}
