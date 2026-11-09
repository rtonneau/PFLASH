// Standalone statistical experiment; reuse the initialized controlled-fixture helpers.
#define main ContactFixtureMain
#include "InjectionReference.cc"
#undef main
#include "TrackPacket.hh"
#include "G4ElectronOccupancy.hh"
#include "G4MoleculeDefinition.hh"
#include "G4TransportationManager.hh"
#include "G4Navigator.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4VSolid.hh"
#include <iomanip>
#include <chrono>
namespace {
std::string outputPath;
std::map<int,unsigned> packetMasks;
std::map<const G4Track*,unsigned> pendingMasks;
unsigned Mask(const G4Track& t) {
 auto p=pendingMasks.find(&t);if(p!=pendingMasks.end()){packetMasks[t.GetTrackID()]=p->second;pendingMasks.erase(p);}
 int a=0,b=0;GetIT(const_cast<G4Track*>(&t))->GetParentID(a,b);
 auto& mask=packetMasks[t.GetTrackID()];mask|=packetMasks[t.GetParentID()]|packetMasks[a]|packetMasks[b];return mask;
}
long long cross=0;
std::map<std::string,long long> yields;
class HistoryAction: public G4UserTimeStepAction {
 public:
 void UserPreTimeStepAction() override {
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) {
   Mask(*t);SourceContacts::Fresh(*t);
   auto it=SourceContacts::activation.find(t->GetTrackID());
   if(it!=SourceContacts::activation.end() && G4Scheduler::Instance()->GetGlobalTime()+1e-15<it->second) premature=true;
  }
 }
 void UserReactionAction(const G4Track& a,const G4Track& b,const std::vector<G4Track*>* products) override {
  const unsigned x=Mask(a),y=Mask(b);
  if(x&&y&&x!=y) ++cross;
  if(products) for(auto* p:*products) pendingMasks[p]=x|y;
 }
 void EndProcessing() override {
  for(auto* t:*G4ITTrackHolder::Instance()->GetMainList()) if(t->GetTrackStatus()!=fStopAndKill) ++yields[GetMolecule(t)->GetName()];
 }
};
const G4MolecularConfiguration* ResolveHistory(const Packets::Molecule& m) {
 for(int i=0;i<G4MolecularConfiguration::GetNumberOfSpecies();++i) {
  auto* c=G4MolecularConfiguration::GetMolecularConfiguration(i);
  if(c->GetDefinition()->GetParticleName()!=m.definition||c->GetCharge()!=m.charge||c->GetDecayTime()!=m.decayTime) continue;
  std::vector<int> occ;if(auto* e=c->GetElectronOccupancy()) for(int j=0;j<e->GetSizeOfOrbit();++j) occ.push_back(e->GetOccupancy(j));
  if(occ==m.occupancy) return c;
 }
 throw std::runtime_error("unknown configuration");
}
}
#ifndef PACKET_HISTORY_HELPERS_ONLY
int main(int argc,char** argv) {
 try {
  if(argc!=9&&argc!=10) throw std::runtime_error("macro packet output seed patched delay_ps copies spectator");
  SourceContacts::enabled=std::stoi(argv[5])!=0;
  SourceContacts::ResetPrivateHistory();
  const double delay=std::stod(argv[6])*ps; const int copies=std::stoi(argv[7]);
  if(delay<0||copies<1||copies>8) throw std::runtime_error("invalid bounded history");
  auto started=std::chrono::steady_clock::now();
  std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;manager->SetUserInitialization(new DetectorConstruction);manager->SetUserInitialization(new PhysicsList);manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]);std::string line,identity;
  if(!macro) throw std::runtime_error("missing macro");
  while(std::getline(macro,line)) {identity+=line+"\n";if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("invalid macro");}
  std::ifstream in(argv[2]);auto packet=Packets::Read(in,std::string(PACKET_COMPATIBILITY)+"|"+identity);
  auto* world=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking()->GetWorldVolume()->GetLogicalVolume();
  if(world->GetMaterial()->GetDensity()*world->GetSolid()->GetCubicVolume()/kg!=packet.massKg) throw std::runtime_error("mass mismatch");
  G4Scheduler::Instance()->SetUserAction(new HistoryAction);
  G4Scheduler::Instance()->SetEndTime(argc==10?std::stod(argv[9])*ps:(copies-1)*delay+3*ps);
  if(std::stoi(argv[8])) Add("O2",0,{100*um,100*um,100*um},999999,0);
  int id=1000000;
  std::vector<const G4MolecularConfiguration*> configs;for(auto& m:packet.molecules) configs.push_back(ResolveHistory(m));
  for(int k=0;k<copies;++k) for(std::size_t i=0;i<packet.molecules.size();++i) {
   auto& m=packet.molecules[i];double activation=m.globalTime+k*delay;
   auto* t=(new G4Molecule(configs[i]))->BuildTrack(activation,{m.position[0],m.position[1],m.position[2]});
   t->SetTrackID(++id);t->SetTrackStatus(static_cast<G4TrackStatus>(m.status));t->SetMomentumDirection({m.direction[0],m.direction[1],m.direction[2]});t->SetKineticEnergy(m.kineticEnergy);t->SetLocalTime(m.localTime);t->SetProperTime(m.properTime);
   SourceContacts::activation[id]=activation;SourceContacts::RegisterPrivateSource(id,activation);packetMasks[id]=1u<<k;G4ITTrackHolder::Instance()->Push(t);
  }
  G4Random::setTheSeed(std::stol(argv[4]));G4DNAChemistryManager::Instance()->UseAsStandalone(true);G4DNAChemistryManager::Instance()->Run();
  if(premature) throw std::runtime_error("premature activation");
  if(std::stoi(argv[8])) --yields["O_2^0"]; // Remove the known distant spectator from yield comparisons.
  std::ofstream out(argv[3]);out<<std::setprecision(17);
  out<<"energy_MeV "<<copies*packet.energyDeposit<<"\nmass_kg "<<packet.massKg<<"\nsource_count "<<copies*packet.molecules.size()<<"\nnew_new_contacts "<<SourceContacts::newNew<<"\nnew_old_contacts "<<SourceContacts::newOld<<"\nold_old_contacts "<<SourceContacts::oldOld<<"\ncross "<<cross<<'\n';
  for(auto [name,n]:yields) out<<std::quoted(name)<<' '<<n<<'\n';
  out<<"seconds "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<'\n';
  if(!out) throw std::runtime_error("output failed");
  std::cout<<"PACKET_HISTORY_PASS\n";return 0;
 }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
#endif


