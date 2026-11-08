#define PACKET_HISTORY_HELPERS_ONLY
#include "PacketHistory.cc"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNABoundingBox.hh"
#include "G4MoleculeCounterManager.hh"
#include <set>
#include <random>
struct TimelineSurvivor{const G4MolecularConfiguration* conf;G4ThreeVector position;};
class TimelineAction final:public G4UserTimeStepAction{
public:
 std::map<int,double> expected;std::set<int> seen;std::vector<TimelineSurvivor> survivors;
 void NewStage() override {const auto now=G4Scheduler::Instance()->GetGlobalTime();for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()){auto it=expected.find(t->GetTrackID());if(it!=expected.end()){if(now+1e-15<it->second)throw std::runtime_error("source activated early");if(!seen.count(it->first)){if(now!=it->second)throw std::runtime_error("source first activation clock mismatch");seen.insert(it->first);}}}}
 void EndProcessing() override {if(seen.size()!=expected.size()||G4ITTrackHolder::Instance()->DelayListsNOTEmpty())throw std::runtime_error("pending source at phase end");for(auto* t:*G4ITTrackHolder::Instance()->GetMainList())if(t->GetTrackStatus()!=fStopAndKill){if(t->GetTrackStatus()!=fAlive)throw std::runtime_error("nonalive survivor");survivors.push_back({GetMolecule(t)->GetMolecularConfiguration(),t->GetPosition()});}}
};
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("macro seed");SourceContacts::enabled=true;SourceContacts::ResetPrivateHistory();
 auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
 std::ifstream in(argv[1]);std::string line;while(std::getline(in,line))if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line))throw std::runtime_error("macro failed");G4MoleculeCounterManager::Instance()->SetIsActive(false);G4Scheduler::Instance()->ResetScavenger(false);G4Random::setTheSeed(std::stol(argv[2]));
 G4DNABoundingBox box({2*um,-2*um,2*um,-2*um,2*um,-2*um});auto* h2=G4MolecularConfiguration::GetMolecularConfiguration("H2");if(!h2)throw std::runtime_error("missing H2");G4DNAEventScheduler meso;meso.SetStartTime(1*ps);meso.SetChangeMesh(false);meso.Initialize(box,160);auto* mesh=meso.GetMesh();mesh->GetVoxelMapList(mesh->GetIndex({0,0,0}))[h2]=1;meso.ProbeRefresh();meso.ProbeAdvanceTo(2*ps);if(mesh->GetNumberOfType(h2)!=1)throw std::runtime_error("old budget mismatch");
 auto* action=new TimelineAction;std::mt19937_64 rng(std::stoull(argv[2])+981723);std::uniform_real_distribution<double> uniform(0.,1.);long count=0;
 for(auto it=mesh->begin();it!=mesh->end();++it)for(auto [conf,n]:std::get<2>(*it))for(size_t j=0;j<n;++j){auto cell=std::get<1>(*it);G4ThreeVector pos(cell.Getxlo()+uniform(rng)*(cell.Getxhi()-cell.Getxlo()),cell.Getylo()+uniform(rng)*(cell.Getyhi()-cell.Getylo()),cell.Getzlo()+uniform(rng)*(cell.Getzhi()-cell.Getzlo()));auto* t=(new G4Molecule(conf))->BuildTrack(2*ps,pos);t->SetTrackID(3000000+ ++count);action->expected[t->GetTrackID()]=2*ps;SourceContacts::activation[t->GetTrackID()]=2*ps;SourceContacts::RegisterPrivateSource(t->GetTrackID(),2*ps);G4ITTrackHolder::Instance()->Push(t);}
 if(count!=1)throw std::runtime_error("reconstruction budget mismatch");mesh->Reset();
 for(int k=0;k<2;++k){const double time=(2.+0.001*k)*ps;auto* t=(new G4Molecule(h2))->BuildTrack(time,{(100+100*k)*nm,0,0});const int id=4000001+k;t->SetTrackID(id);action->expected[id]=time;SourceContacts::activation[id]=time;SourceContacts::RegisterPrivateSource(id,time);G4ITTrackHolder::Instance()->Push(t);}
 G4Scheduler::Instance()->SetUserAction(action);G4Scheduler::Instance()->SetEndTime(5*ps);G4DNAChemistryManager::Instance()->UseAsStandalone(true);G4DNAChemistryManager::Instance()->Run();if(action->survivors.size()!=3)throw std::runtime_error("particle population mismatch");
 meso.ProbeRefresh();meso.ProbeAdvanceTo(5*ps);for(auto& item:action->survivors)++mesh->GetVoxelMapList(mesh->GetIndex(item.position))[item.conf];meso.ProbeRefresh();if(mesh->GetNumberOfType(h2)!=3)throw std::runtime_error("handover population mismatch");meso.ProbeAdvanceTo(6*ps);if(mesh->GetNumberOfType(h2)!=3||meso.GetGlobalTime()!=6*ps)throw std::runtime_error("final mesh budget/clock mismatch");std::cout<<"ORDERED_THREE_SOURCE_PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
