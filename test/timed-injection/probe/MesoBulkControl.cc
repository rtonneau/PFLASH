#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4DNAChemistryManager.hh"
#include "G4Scheduler.hh"
#include "G4MoleculeTable.hh"
#include "G4Molecule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4ITTrackHolder.hh"
#include "G4UserTimeStepAction.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "SourceContacts.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "actions/ActionInitialization.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "Randomize.hh"
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
struct BulkSurvivor{const G4MolecularConfiguration* conf;G4ThreeVector position;};
class BulkAction final:public G4UserTimeStepAction{public:double freshTime=0;bool registered=false;std::vector<BulkSurvivor> survivors;
 void NewStage() override {for(auto* t:*G4ITTrackHolder::Instance()->GetMainList())if(t->GetTrackID()==9000002){auto now=G4Scheduler::Instance()->GetGlobalTime();if(now<freshTime)throw std::runtime_error("bulk fixture source early");if(!registered){if(now!=freshTime)throw std::runtime_error("bulk source time mismatch");registered=true;}}}
 void EndProcessing() override{if(!registered)throw std::runtime_error("bulk fixture source missing");for(auto* t:*G4ITTrackHolder::Instance()->GetMainList())if(t->GetTrackStatus()!=fStopAndKill){if(t->GetTrackStatus()!=fAlive)throw std::runtime_error("unresolved bulk fixture state");survivors.push_back({GetMolecule(t)->GetMolecularConfiguration(),t->GetPosition()});}}
};
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("oxygen macro required");auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
 std::ifstream in(argv[1]);std::string line;while(std::getline(in,line))if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line))throw std::runtime_error("macro failed");G4MoleculeCounterManager::Instance()->SetIsActive(false);
 auto* bulk=dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial());auto* o2=G4MoleculeTable::Instance()->GetConfiguration("O2");auto* electron=G4MoleculeTable::Instance()->GetConfiguration("e_aq");if(!bulk||!o2||!electron)throw std::runtime_error("missing bulk or species");const auto original=bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(o2);bool found=false;for(auto it=bulk->begin();it!=bulk->end();++it){if(it->first==o2){it->second=1000000;found=true;}else it->second=0;}if(!found)throw std::runtime_error("oxygen pool absent");
 G4DNABoundingBox box({2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um});G4DNAEventScheduler meso;meso.SetStartTime(CLHEP::ps);meso.SetChangeMesh(false);meso.Initialize(box,1);auto* mesh=meso.GetMesh();auto index=mesh->GetIndex({0,0,0});mesh->GetVoxelMapList(index)[electron]=1;G4Random::setTheSeed(1729);meso.ProbeResetBulkRefreshCount();meso.ProbeRefresh();
 auto next=meso.ProbeNextEventTime();if(!std::isfinite(next)||next==DBL_MAX)throw std::runtime_error("missing first reaction");meso.ProbeAdvanceTo(std::nextafter(next,INFINITY));auto afterFirst=bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(o2);auto refreshFirst=meso.ProbeBulkRefreshCount();if(afterFirst!=999999||refreshFirst<1)throw std::runtime_error("first bulk reaction or global refresh failed");
 const auto origin=meso.GetGlobalTime();SourceContacts::enabled=true;SourceContacts::ResetPrivateHistory();long id=9000000;for(auto it=mesh->begin();it!=mesh->end();++it)for(auto [conf,n]:std::get<2>(*it))for(size_t j=0;j<n;++j){auto* t=(new G4Molecule(conf))->BuildTrack(origin,{0,0,0});t->SetTrackID(++id);SourceContacts::activation[id]=origin;SourceContacts::RegisterPrivateSource(id,origin);G4ITTrackHolder::Instance()->Push(t);}if(id!=9000001)throw std::runtime_error("unexpected old bulk products");mesh->Reset();meso.ProbeRefresh();
 const auto fresh=origin+CLHEP::ps, end=origin+2*CLHEP::ps;auto* t=(new G4Molecule(electron))->BuildTrack(fresh,{0.1*CLHEP::nm,0,0});t->SetTrackID(9000002);SourceContacts::activation[9000002]=fresh;SourceContacts::RegisterPrivateSource(9000002,fresh);G4ITTrackHolder::Instance()->Push(t);auto* action=new BulkAction;action->freshTime=fresh;G4Scheduler::Instance()->SetUserAction(action);G4Scheduler::Instance()->ResetScavenger(false);G4Scheduler::Instance()->SetEndTime(end);G4DNAChemistryManager::Instance()->UseAsStandalone(true);G4DNAChemistryManager::Instance()->Run();
 const auto afterParticle=bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(o2);if(G4Scheduler::Instance()->GetScavengerMaterial()!=bulk||afterParticle!=afterFirst)throw std::runtime_error("depleted bulk changed/reset across particle phase");meso.ProbeAdvanceTo(end);for(auto& a:action->survivors)++mesh->GetVoxelMapList(mesh->GetIndex(a.position))[a.conf];meso.ProbeRefresh();next=meso.ProbeNextEventTime();if(!std::isfinite(next)||next==DBL_MAX)throw std::runtime_error("missing second reaction");meso.ProbeAdvanceTo(std::nextafter(next,INFINITY));auto afterSecond=bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(o2);auto refreshSecond=meso.ProbeBulkRefreshCount();if(afterSecond!=999998||refreshSecond<=refreshFirst)throw std::runtime_error("depleted bulk continuity or second refresh failed");
 std::cout<<"BULK_CONTROL_PASS original="<<original<<" engineered=1000000 after1="<<afterFirst<<" afterParticle="<<afterParticle<<" after2="<<afterSecond<<" refresh1="<<refreshFirst<<" refresh2="<<refreshSecond<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
