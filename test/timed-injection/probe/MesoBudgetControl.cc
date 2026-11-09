#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeTable.hh"
#include "G4MoleculeCounterManager.hh"
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
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("macro required");
 auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
 std::ifstream in(argv[1]);std::string line;while(std::getline(in,line))if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line))throw std::runtime_error("macro failed");G4MoleculeCounterManager::Instance()->SetIsActive(false);
 G4DNABoundingBox box({2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um});G4DNAEventScheduler meso;meso.SetStartTime(CLHEP::ps);meso.SetChangeMesh(false);meso.Initialize(box,160);
 auto* conf=G4MoleculeTable::Instance()->GetConfiguration("H2");if(!conf)throw std::runtime_error("missing H2");auto* mesh=meso.GetMesh();mesh->GetVoxelMapList(mesh->GetIndex({0,0,0}))[conf]=8;meso.ProbeRefresh();
 const auto sourceTime=2*CLHEP::ps;meso.ProbeAdvanceTo(sourceTime);if(meso.GetGlobalTime()!=sourceTime||mesh->GetNumberOfType(conf)!=8)throw std::runtime_error("pre-source budget or clock mismatch");
 mesh->GetVoxelMapList(mesh->GetIndex({100*CLHEP::nm,0,0}))[conf]+=3;meso.ProbeRefresh();if(mesh->GetNumberOfType(conf)!=11)throw std::runtime_error("source budget mismatch");
 meso.ReVoxelizing(80);mesh=meso.GetMesh();meso.ProbeRefresh();if(meso.GetPixels()!=80||mesh->GetNumberOfType(conf)!=11)throw std::runtime_error("coarsening population mismatch");
 meso.ProbeAdvanceTo(3*CLHEP::ps);if(meso.GetGlobalTime()!=3*CLHEP::ps||mesh->GetNumberOfType(conf)!=11)throw std::runtime_error("post-source budget or clock mismatch");
 std::cout<<"MESO_BUDGET_PASS old=8 source=3 final=11\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
