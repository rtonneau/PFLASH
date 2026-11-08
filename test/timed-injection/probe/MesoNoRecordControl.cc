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
 if(argc!=3)throw std::runtime_error("macro empty|reaction|arrival");std::string mode=argv[2];if(mode!="empty"&&mode!="reaction"&&mode!="arrival")throw std::runtime_error("invalid fixture");
 auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
 std::ifstream in(argv[1]);std::string line;while(std::getline(in,line))if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line))throw std::runtime_error("macro failed");G4MoleculeCounterManager::Instance()->SetIsActive(false);
 G4DNABoundingBox box({12.5*CLHEP::nm,-12.5*CLHEP::nm,12.5*CLHEP::nm,-12.5*CLHEP::nm,12.5*CLHEP::nm,-12.5*CLHEP::nm});
 G4DNAEventScheduler meso;meso.SetStartTime(CLHEP::ps);meso.SetChangeMesh(false);meso.Initialize(box,1);
 if(mode=="empty"){meso.ProbeRefresh();meso.ProbeAdvanceTo(10*CLHEP::ps);if(meso.GetGlobalTime()!=10*CLHEP::ps)throw std::runtime_error("empty queue clock mismatch");}
 else {auto* conf=G4MoleculeTable::Instance()->GetConfiguration("e_aq");if(!conf)throw std::runtime_error("missing electron");auto* mesh=meso.GetMesh();mesh->GetVoxelMapList(mesh->GetIndex({0,0,0}))[conf]=(mode=="arrival"?1:2);meso.ProbeRefresh();if(mode=="arrival"){meso.ProbeAdvanceTo(2*CLHEP::ps);if(mesh->GetNumberOfType(conf)!=1)throw std::runtime_error("source executed early or old population changed");++mesh->GetVoxelMapList(mesh->GetIndex({0,0,0}))[conf];meso.ProbeRefresh();}meso.SetEndTime(CLHEP::second);meso.Stepping();if(mesh->GetNumberOfType(conf)>=2)throw std::runtime_error("expected reaction did not fire");}
 std::cout<<"NO_RECORD_CONTROL_PASS "<<mode<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
