#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "G4DNAIndependentReactionTimeStepper.hh"
#include "G4DNAChemistryManager.hh"
#include "G4MoleculeCounter.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4Molecule.hh"
#include "G4ITTrackHolder.hh"
#include "G4Scheduler.hh"
#include "G4UserTimeStepAction.hh"
#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
#include "G4UserStackingAction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4RunManagerFactory.hh"
#include "G4MTRunManager.hh"
#include "G4Event.hh"
#include "G4Run.hh"
#include "G4Geantino.hh"
#include "G4PrimaryVertex.hh"
#include "G4PrimaryParticle.hh"
#include "G4UImanager.hh"
#include "G4Threading.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include "TrackPacket.hh"
#include "BeamSchedule.hh"
#include "G4ElectronOccupancy.hh"
#include "G4MoleculeDefinition.hh"
#include <iomanip>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace {
struct PacketInput {
  std::vector<Packets::Packet> library;
  double rate=0;
};
struct HistoryState {
  int event=0, run=0, reactions=0, registered=0, survivors=0;
  bool ran=false;
  std::set<int> pending;
  std::shared_ptr<const PacketInput> input;
  std::vector<BeamSchedule::Arrival> arrivals;
  std::map<std::string,int> yields;
  std::size_t sourceCount=0;
  double energy=0;
  long seed=0;
};
const G4MolecularConfiguration* ResolvePacket(const Packets::Molecule& molecule) {
  for(int i=0;i<G4MolecularConfiguration::GetNumberOfSpecies();++i) {
    auto* config=G4MolecularConfiguration::GetMolecularConfiguration(i);
    if(config->GetDefinition()->GetParticleName()!=molecule.definition || config->GetCharge()!=molecule.charge || config->GetDecayTime()!=molecule.decayTime) continue;
    std::vector<int> occupancy;
    if(auto* electrons=config->GetElectronOccupancy()) for(int j=0;j<electrons->GetSizeOfOrbit();++j) occupancy.push_back(electrons->GetOccupancy(j));
    if(occupancy==molecule.occupancy) return config;
  }
  throw std::runtime_error("unknown packet configuration");
}
class ChemistryOnlyDetector : public DetectorConstruction {
public:
  void ConstructSDandField() override {}
};
class CounterRun : public G4UserRunAction {
public:
  void BeginOfRunAction(const G4Run* run) override { G4DNAChemistryManager::Instance()->BeginOfRunAction(run); }
  void EndOfRunAction(const G4Run* run) override { G4DNAChemistryManager::Instance()->EndOfRunAction(run); }
};
class DummyPrimary : public G4VUserPrimaryGeneratorAction {
public:
  void GeneratePrimaries(G4Event* event) override {
    auto* vertex=new G4PrimaryVertex(G4ThreeVector(),0);
    vertex->SetPrimary(new G4PrimaryParticle(G4Geantino::Definition(),0,0,1*keV));
    event->AddPrimaryVertex(vertex);
  }
};
class HistoryTimeAction : public G4UserTimeStepAction {
  std::shared_ptr<HistoryState> state;
public:
  explicit HistoryTimeAction(std::shared_ptr<HistoryState> value):state(std::move(value)){}
  void NewStage() override {
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList()) {
      if(state->pending.erase(track->GetTrackID())) {
        if(track->GetGlobalTime()!=G4Scheduler::Instance()->GetGlobalTime()) throw std::runtime_error("early MT activation");
        G4MoleculeCounterManager::Instance()->AddMolecule(track,track->GetGlobalTime());
        ++state->registered;
      }
    }
  }
  void UserReactionAction(const G4Track&,const G4Track&,const std::vector<G4Track*>*) override {
    const auto time=G4Scheduler::Instance()->GetGlobalTime();
    const auto offset=(state->event%3)*ps;
    if(!state->input && time!=10*ps+offset && time!=20*ps+offset) throw std::runtime_error("unexpected synthetic reaction time");
    ++state->reactions;
  }
  void EndProcessing() override {
    std::map<std::string,int> live;
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList())
      if(track->GetTrackStatus()!=fStopAndKill){++state->survivors;++live[GetMolecule(track)->GetName()];}
    if(state->registered!=state->sourceCount || !state->pending.empty() || (!state->input && (state->reactions!=2 || state->survivors!=7)))
      throw std::runtime_error("complete history budget/state failure");
    const auto counters=G4MoleculeCounterManager::Instance()->GetMoleculeCounters("BasicCounter");
    if(counters.size()!=1) throw std::runtime_error("MT counter missing");
    const auto* counter=dynamic_cast<const G4MoleculeCounter*>(counters.front());
    if(!counter) throw std::runtime_error("MT counter type mismatch");
    for(const auto* molecule:counter->GetRecordedMolecules())
      if(counter->GetNbMoleculesAtTime(G4MoleculeCounterIndex(molecule),G4Scheduler::Instance()->GetEndTime())!=live[molecule->GetName()])
        throw std::runtime_error("MT counter/live population mismatch");
    state->yields=std::move(live);
  }
};
class HistoryEvent : public G4UserEventAction {
  std::shared_ptr<HistoryState> state;
  std::string directory;
public:
  HistoryEvent(std::shared_ptr<HistoryState> value,std::string path):state(std::move(value)),directory(std::move(path)){}
  void BeginOfEventAction(const G4Event* event) override {
    auto input=state->input;
    *state=HistoryState{};
    state->input=std::move(input);
    state->event=event->GetEventID();
    state->run=G4RunManager::GetRunManager()->GetCurrentRun()->GetRunID();
    G4DNAIndependentReactionTimeStepper::ResetSourceActivations();
    G4DNAChemistryManager::Instance()->BeginOfEventAction(event);
  }
  void EndOfEventAction(const G4Event* event) override {
    if(!state->ran || state->registered!=state->sourceCount) throw std::runtime_error("history did not run");
    std::ofstream out(directory+"/run"+std::to_string(state->run)+"event"+std::to_string(state->event)+".txt");
    out<<"run "<<state->run<<"\nevent "<<state->event<<"\nworker "<<G4Threading::G4GetThreadId()
       <<"\nregistered "<<state->registered<<"\nreactions "<<state->reactions<<"\nsurvivors "<<state->survivors
       <<"\noffset_ps "<<state->event%3<<"\nCOMPLETE_HISTORY_PASS\n";
    if(state->input) {
      out<<std::setprecision(17)<<"seed "<<state->seed<<"\npackets "<<state->arrivals.size()<<"\nsource_count "<<state->sourceCount<<"\nenergy_MeV "<<state->energy<<"\ndose_Gy "<<BeamSchedule::Dose(state->energy,state->input->library.front().massKg)<<'\n';
      for(const auto& arrival:state->arrivals) out<<"arrival "<<arrival.packet<<' '<<arrival.time*second/ps<<'\n';
      for(const auto& [name,count]:state->yields) out<<std::quoted(name)<<' '<<count<<'\n';
    }
    if(!out) throw std::runtime_error("history output failed");
    G4DNAChemistryManager::Instance()->EndOfEventAction(event);
  }
};
class HistoryStack : public G4UserStackingAction {
  std::shared_ptr<HistoryState> state;
public:
  explicit HistoryStack(std::shared_ptr<HistoryState> value):state(std::move(value)){}
  void NewStage() override {
    if(state->ran) return;
    state->ran=true;
    auto* counters=G4MoleculeCounterManager::Instance();
    if(!counters->GetIsActive()) throw std::runtime_error("MT counters inactive");
    counters->SetIsActive(false);
    if(state->input) {
      const long seeds[]={90001,90002,90003,90005};
      state->seed=seeds[state->run*2+state->event];
      std::mt19937_64 arrivals(state->seed),selection(state->seed+1);
      state->arrivals=BeamSchedule::Sample(state->input->rate,100*ps/second,state->input->library.size(),arrivals,selection);
      if(state->arrivals.size()>8) throw std::runtime_error("MT schedule resource bound exceeded");
      int id=1000000;
      for(const auto& arrival:state->arrivals) {
        const auto& packet=state->input->library.at(arrival.packet);
        state->energy+=packet.energyDeposit;
        for(const auto& molecule:packet.molecules) {
          const double time=arrival.time*second+molecule.globalTime;
          if(time>=102*ps) throw std::runtime_error("MT source beyond observation window");
          auto* track=(new G4Molecule(ResolvePacket(molecule)))->BuildTrack(time,{molecule.position[0],molecule.position[1],molecule.position[2]});
          track->SetTrackID(++id);track->SetTrackStatus(static_cast<G4TrackStatus>(molecule.status));
          track->SetMomentumDirection({molecule.direction[0],molecule.direction[1],molecule.direction[2]});
          track->SetKineticEnergy(molecule.kineticEnergy);track->SetLocalTime(molecule.localTime);track->SetProperTime(molecule.properTime);
          state->pending.insert(id);
          G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(id,time);
          G4ITTrackHolder::Instance()->Push(track);
          ++state->sourceCount;
        }
      }
      counters->SetIsActive(true);
      G4Random::setTheSeed(state->seed+2);
      G4Scheduler::Instance()->SetEndTime(102*ps);
      if(!state->arrivals.empty()) G4DNAChemistryManager::Instance()->Run();
      return;
    }
    auto add=[&](const char* name,double time,double x,int id){
      auto* config=G4MolecularConfiguration::GetMolecularConfiguration(name);
      if(!config) throw std::runtime_error("missing MT species");
      auto* track=(new G4Molecule(config))->BuildTrack(time,{x,0,0});
      track->SetTrackID(id);track->SetTrackStatus(fAlive);
      state->pending.insert(id);
      G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(id,time);
      G4ITTrackHolder::Instance()->Push(track);
    };
    const auto offset=(state->event%3)*ps;
    add("O2",1*ps,100*um,1000001);
    add("e_aq",10*ps+offset,0,1000002);
    add("e_aq",10*ps+offset,0.01*nm,1000003);
    add("e_aq",20*ps+offset,1*um,1000004);
    add("e_aq",20*ps+offset,1*um+0.01*nm,1000005);
    state->sourceCount=5;
    counters->SetIsActive(true);
    G4Random::setTheSeed(55001+state->run*10000+state->event*101);
    G4Scheduler::Instance()->SetEndTime(30*ps);
    G4DNAChemistryManager::Instance()->Run();
  }
};
class HistoryInitialization : public ActionInitialization {
  std::string directory;
  std::shared_ptr<const PacketInput> input;
public:
  explicit HistoryInitialization(std::string path,std::shared_ptr<const PacketInput> packets):directory(std::move(path)),input(std::move(packets)){}
  void BuildForMaster() const override {SetUserAction(new CounterRun);BuildMoleculeCounters();}
  void Build() const override {
    auto state=std::make_shared<HistoryState>();
    state->input=input;
    SetUserAction(new DummyPrimary);
    SetUserAction(new CounterRun);
    SetUserAction(new HistoryEvent(state,directory));
    SetUserAction(new HistoryStack(state));
    BuildMoleculeCounters();
    G4Scheduler::Instance()->SetUserAction(new HistoryTimeAction(state));
  }
};
}
int main(int argc,char** argv) {
  try {
    if(argc!=4 && argc!=5) throw std::runtime_error("macro output threads (0=Serial) [packet-library]");
    const int threads=std::stoi(argv[3]);
    if(threads<0 || threads>2) throw std::runtime_error("bounded harness supports Serial or up to two workers");
    std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(threads?G4RunManagerType::MT:G4RunManagerType::Serial));
    if(threads) dynamic_cast<G4MTRunManager*>(manager.get())->SetNumberOfThreads(threads);
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new ChemistryOnlyDetector);
    manager->SetUserInitialization(new PhysicsList);
    std::shared_ptr<PacketInput> input;
    if(argc==5) {
      input=std::make_shared<PacketInput>();
      std::ifstream identityFile(argv[1]);std::string line,identity;
      if(!identityFile) throw std::runtime_error("missing packet identity macro");
      while(std::getline(identityFile,line)) identity+=line+"\n";
      double mean=0;
      for(int i=0;i<8;++i) {
        std::ifstream packet(std::string(argv[4])+"/library"+std::to_string(i)+".packet");
        input->library.push_back(Packets::Read(packet,std::string(PACKET_COMPATIBILITY)+"|"+identity));
        mean+=input->library.back().energyDeposit/8;
        if(input->library.back().massKg!=input->library.front().massKg) throw std::runtime_error("inconsistent packet mass");
      }
      input->rate=BeamSchedule::Rate(1000,input->library.front().massKg,mean);
    }
    manager->SetUserInitialization(new HistoryInitialization(argv[2],input));
    std::ifstream macro(argv[1]);std::string line;
    if(!macro) throw std::runtime_error("missing macro");
    while(std::getline(macro,line)) {
      if(line.rfind("/gun/",0)==0 || line.rfind("/primaryKiller/",0)==0) continue;
      if(!line.empty()&&G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
    }
    manager->BeamOn(input?2:8);
    manager->BeamOn(input?2:8);
    return 0;
  } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
