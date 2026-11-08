#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeTable.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "actions/ActionInitialization.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
int main(int argc, char** argv) {
 try {
  if (argc != 2) throw std::runtime_error("expected macro");
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;
  manager->SetUserInitialization(new DetectorConstruction);
  manager->SetUserInitialization(new PhysicsList);
  manager->SetUserInitialization(new ActionInitialization);
  std::ifstream in(argv[1]); std::string line;
  while (std::getline(in,line)) if (!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
#ifdef _WIN32
  HMODULE modules[2048]; DWORD bytes=0;
  if (!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes) || bytes>sizeof(modules)) throw std::runtime_error("module inventory failed");
  for (DWORD i=0;i<bytes/sizeof(HMODULE);++i) { char path[MAX_PATH]; if (!GetModuleFileNameExA(GetCurrentProcess(),modules[i],path,MAX_PATH)) throw std::runtime_error("module path failed"); std::cout << "MODULE " << path << "\n"; }
#endif
  G4DNABoundingBox box({250*CLHEP::um,-250*CLHEP::um,250*CLHEP::um,-250*CLHEP::um,250*CLHEP::um,-250*CLHEP::um});
  G4DNAEventScheduler scheduler;
  scheduler.Initialize(box,2);
  auto* mesh=scheduler.GetMesh();
  auto* species=G4MoleculeTable::Instance()->GetConfiguration("OH");
  if (!species) throw std::runtime_error("OH missing");
  mesh->GetVoxelMapList(mesh->GetIndex({0,0,0}))[species]=8;
  auto before=mesh->GetNumberOfType(species);
  scheduler.Voxelizing(box);
  auto after=scheduler.GetMesh()->GetNumberOfType(species);
  std::cout << "REENTRY_OLD_POPULATION " << before << " " << after << std::endl;
  if (before != 8 || after != 0) throw std::runtime_error("unexpected diagnostic outcome");
  return 0;
 } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 2; }
}
