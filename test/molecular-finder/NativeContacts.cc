#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeDefinition.hh"
#include "G4DNAMakeReaction.hh"
#include "G4DiffusionControlledReactionModel.hh"
#include "G4VExceptionHandler.hh"
#include "G4StateManager.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4UserTimeStepAction.hh"
#include "Randomize.hh"
#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int reactions=0, electrons=0, electronBalance=0;
const G4MolecularConfiguration* electronConfiguration=nullptr;
struct GuardObserved {};
class GuardHandler : public G4VExceptionHandler {
 public:
  G4bool Notify(const char*,const char* code,G4ExceptionSeverity,const char*) override {
    if(std::string(code)=="G4DNAMakeReaction003") throw GuardObserved{};
    throw std::runtime_error(std::string("unexpected guard exception: ")+code);
  }
};
class ContactAction : public G4UserTimeStepAction {
 public:
  void UserReactionAction(const G4Track& a,const G4Track& b,const std::vector<G4Track*>* products) override {
    for(const auto* track : {&a,&b}) if(GetMolecule(track)->GetMolecularConfiguration()==electronConfiguration) --electronBalance;
    if(products) for(auto* track:*products) if(GetMolecule(track)->GetMolecularConfiguration()==electronConfiguration) ++electronBalance;
    if (&a == &b) throw std::runtime_error("self reaction");
    if (GetMolecule(a)->GetMolecularConfiguration()==electronConfiguration && GetMolecule(b)->GetMolecularConfiguration()==electronConfiguration) {
      ++reactions;
      if(G4Scheduler::Instance()->GetGlobalTime()!=1*ps) throw std::runtime_error("contact did not react at start");
      std::cout << "CONTACT_AT " << G4Scheduler::Instance()->GetGlobalTime()/ps << '\n';
    }
  }
  void EndProcessing() override {
    for(auto* track:*G4ITTrackHolder::Instance()->GetMainList())
      if(track->GetTrackStatus()!=fStopAndKill && GetMolecule(track)->GetMolecularConfiguration()==electronConfiguration) ++electrons;
  }
};
}
int main(int argc,char** argv) {
 try {
  if(argc!=5) throw std::runtime_error("macro count separation_nm seed");
  const int count=std::stoi(argv[2]);
  const double separation=std::stod(argv[3])*nm;
  if(count< -2 || count>4) throw std::runtime_error("count out of range");
  auto manager=std::unique_ptr<G4RunManager>(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;
  manager->SetUserInitialization(new DetectorConstruction);
  manager->SetUserInitialization(new PhysicsList);
  manager->SetUserInitialization(new ActionInitialization);
  std::ifstream macro(argv[1]); if(!macro) throw std::runtime_error("missing macro");
  std::string line;
  while(std::getline(macro,line)) if(!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line)) throw std::runtime_error("macro failed");
  G4MoleculeCounterManager::Instance()->SetIsActive(false);
  auto* electron=G4MolecularConfiguration::GetMolecularConfiguration("e_aq");
  electronConfiguration=electron;
  const auto* reaction=G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(electron,electron);
  if(!reaction || reaction->GetProbability()!=1 || reaction->GetEffectiveReactionRadius()<=0.03*nm) throw std::runtime_error("invalid contact assumptions");
  if(count<0) {
    auto a=std::unique_ptr<G4Track>((new G4Molecule(electron))->BuildTrack(1*ps,{0,0,0}));
    auto b=std::unique_ptr<G4Track>((new G4Molecule(electron))->BuildTrack(1*ps,{0,0,0}));
    G4DiffusionControlledReactionModel model;
    model.SetReactionTable(G4DNAMolecularReactionTable::GetReactionTable());
    G4DNAMakeReaction process(&model);
    process.TestReactibility(*a,*b,count==-1?1*ps:0,false);
    auto* state=G4StateManager::GetStateManager();
    auto* previous=state->GetExceptionHandler();
    bool observed=false;
    GuardHandler handler;
    try {process.UpdatePositionForReaction(*a,count==-2?*a:*b);}
    catch(const GuardObserved&) {observed=true;}
    catch(...) {state->SetExceptionHandler(previous);throw;}
    state->SetExceptionHandler(previous);
    if(!observed) throw std::runtime_error("reaction guard was bypassed");
    std::cout << "GUARD_PASS " << (count==-1?"positive-time":"self") << '\n';
    return 0;
  }
  if(count==0) {
    const G4MolecularConfiguration* hole=nullptr;
    for(int i=0;i<G4MolecularConfiguration::GetNumberOfSpecies();++i) {
      auto* configuration=G4MolecularConfiguration::GetMolecularConfiguration(i);
      if(configuration->GetDefinition()->GetParticleName()=="H2O" && configuration->GetCharge()==1) {hole=configuration;break;}
    }
    if(!hole) throw std::runtime_error("missing ionized water");
    auto* track=(new G4Molecule(hole))->BuildTrack(1*ps,{0.123*nm,0.23*nm,0.37*nm});
    track->SetTrackID(1000000);
    track->SetTrackStatus(fStopButAlive);
    G4ITTrackHolder::Instance()->Push(track);
  }
  for(int i=0;i<(count==0?1:count);++i) {
    auto* track=(new G4Molecule(electron))->BuildTrack(1*ps,{0.123*nm+i*separation,0.23*nm,0.37*nm});
    track->SetTrackID(1000001+i);
    G4ITTrackHolder::Instance()->Push(track);
  }
  G4Random::setTheSeed(std::stol(argv[4]));
  G4Scheduler::Instance()->SetUserAction(new ContactAction);
  G4Scheduler::Instance()->SetEndTime(1.001*ps);
  G4DNAChemistryManager::Instance()->UseAsStandalone(true);
  G4DNAChemistryManager::Instance()->Run();
  std::cout << "COUNTS reactions " << reactions << " electrons " << electrons << std::endl;
  const int expected=separation<0.1*nm ? count/2 : 0;
  if(count==0) {
    if(electrons!=0) throw std::runtime_error("coincident electron-hole contact missed");
  } else {
    if(electrons!=count+electronBalance) throw std::runtime_error("electron reaction balance mismatch");
    if((count<=2 || separation>=0.1*nm) ? reactions!=expected : reactions<1)
      throw std::runtime_error("native contact population mismatch");
  }
  std::cout << "NATIVE_PASS reactions " << reactions << " electrons " << electrons << '\n';
  return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
