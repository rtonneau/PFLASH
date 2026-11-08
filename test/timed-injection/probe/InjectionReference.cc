#include "SourceContacts.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeDefinition.hh"
#include "G4DNAScavengerMaterial.hh"
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
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
struct Ancestry
{
  unsigned mask;
  double activation;
};
std::map<int, Ancestry> ancestry;
int crossReactions = 0;
int overlappingReactions = 0;
std::map<const G4Track*, Ancestry> pendingAncestry;
Ancestry MergeAncestry(Ancestry a, Ancestry b) { return {a.mask | b.mask, std::max(a.activation,b.activation)}; }
int totalReactions = 0;
int descendantCrossReactions = 0;
void ResolveAncestry(const G4Track& track)
{
  auto pending=pendingAncestry.find(&track);
  if (pending!=pendingAncestry.end()) {
    ancestry[track.GetTrackID()]=pending->second;
    pendingAncestry.erase(pending);
  }
  int first=0,second=0;
  GetIT(const_cast<G4Track*>(&track))->GetParentID(first,second);
  Ancestry combined{0,0};
  auto own=ancestry.find(track.GetTrackID());
  if(own!=ancestry.end()) combined=own->second;
  for (const auto id : {track.GetParentID(),first,second}) {
    auto parent=ancestry.find(id);
    if (id != 0 && parent != ancestry.end()) combined=MergeAncestry(combined,parent->second);
  }
  if(combined.mask) ancestry[track.GetTrackID()]=combined;
}
bool premature = false;
int survivors = 0;
bool checkBulk = false;
bool checkScore = false;
G4DNAScavengerMaterial* originalBulk = nullptr;
double depletedBulkCount = 0;
int bulkChecks = 0;
void CheckBulkContinuity()
{
  if (!checkBulk) return;
  auto* bulk = dynamic_cast<G4DNAScavengerMaterial*>(G4Scheduler::Instance()->GetScavengerMaterial());
  auto* oxygen = G4MolecularConfiguration::GetMolecularConfiguration("O2");
  if (!bulk || !oxygen) throw std::runtime_error("bulk fixture unavailable");
  if (!originalBulk) {
    originalBulk = bulk;
    const auto initial = bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(oxygen);
    if (initial < 8) throw std::runtime_error("bulk fixture needs at least eight molecules");
    // Controlled depletion marker: a reset at arrival would restore these seven.
    for (int i=0;i<7;++i)
      bulk->ReduceNumberMoleculePerVolumeUnitForMaterialConf(oxygen,G4Scheduler::Instance()->GetGlobalTime());
    depletedBulkCount = initial-7;
  }
  if (bulk != originalBulk || bulk->GetNumberMoleculePerVolumeUnitForMaterialConf(oxygen) != depletedBulkCount)
    throw std::runtime_error("bulk state reset across source activation");
  ++bulkChecks;
  std::cout << "BULK_CONTINUITY " << G4Scheduler::Instance()->GetGlobalTime()/ps
            << " count " << depletedBulkCount << '\n';
}
class InjectionAction : public G4UserTimeStepAction
{
public:
  void NewStage() override
  {
    CheckBulkContinuity();
    if (checkScore && G4Scheduler::Instance()->GetGlobalTime() == 10*ps && crossReactions != 1)
      throw std::runtime_error("reaction score lost before second activation");
    std::cout << "STAGE " << G4Scheduler::Instance()->GetGlobalTime() / ps;
    for (auto *track : *G4ITTrackHolder::Instance()->GetMainList())
    {
      std::cout << " " << track->GetTrackID() << ":" << GetMolecule(track)->GetName();
      auto a = ancestry.find(track->GetTrackID());
      if (a != ancestry.end() &&
          G4Scheduler::Instance()->GetGlobalTime() + 1e-15 < a->second.activation)
        premature = true;
    }
    std::cout << '\n';
  }
  void UserPreTimeStepAction() override
  {
    for (auto *track : *G4ITTrackHolder::Instance()->GetMainList())
    {
      auto a = ancestry.find(track->GetTrackID());
      if (a != ancestry.end() &&
          G4Scheduler::Instance()->GetGlobalTime() + 1e-15 < a->second.activation)
        premature = true;
    }
  }
  void UserReactionAction(const G4Track &a, const G4Track &b,
                          const std::vector<G4Track *> *products) override
  {
    ++totalReactions;
    const bool descendant = ancestry.find(a.GetTrackID()) == ancestry.end() || ancestry.find(b.GetTrackID()) == ancestry.end();
    ResolveAncestry(a);
    ResolveAncestry(b);
    auto aa = ancestry.find(a.GetTrackID()), bb = ancestry.find(b.GetTrackID());
    if (aa != ancestry.end() && bb != ancestry.end())
    {
      if (aa->second.mask && bb->second.mask && !(aa->second.mask & bb->second.mask))
      {
        ++crossReactions;
        if (descendant && G4Scheduler::Instance()->GetGlobalTime() == std::max(aa->second.activation,bb->second.activation)) {
          ++descendantCrossReactions;
          std::cout << "DESCENDANT_REACTION " << G4Scheduler::Instance()->GetGlobalTime()/ps
                    << " ids " << a.GetTrackID() << " " << b.GetTrackID() << '\n';
        }
      }
      if ((aa->second.mask & bb->second.mask) && ((aa->second.mask | bb->second.mask) != aa->second.mask || (aa->second.mask & (aa->second.mask-1)))) ++overlappingReactions;
      if (products) for(auto* product:*products) pendingAncestry[product]=MergeAncestry(aa->second,bb->second);
      if (G4Scheduler::Instance()->GetGlobalTime() + 1e-15 <
          std::max(aa->second.activation, bb->second.activation))
        premature = true;
      std::cout << "REACTION " << G4Scheduler::Instance()->GetGlobalTime() / ps << " packets "
                << aa->second.mask << " " << bb->second.mask << '\n';
    }
  }
  void EndProcessing() override
  {
    CheckBulkContinuity();
    for (auto *track : *G4ITTrackHolder::Instance()->GetMainList())
      if (track->GetTrackStatus() != fStopAndKill)
      {
        ++survivors;
        std::cout << "SURVIVOR " << track->GetTrackID() << " " << GetMolecule(track)->GetName()
                  << " t_ps " << track->GetGlobalTime() / ps << '\n';
      }
    std::cout << "END " << G4Scheduler::Instance()->GetGlobalTime() / ps << " cross "
              << crossReactions << " overlapping " << overlappingReactions << " premature " << premature << " survivors " << survivors << '\n';
  }
};
void Add(const std::string &species, double time, const G4ThreeVector &pos, int id, int packet)
{
  auto *config = G4MolecularConfiguration::GetMolecularConfiguration(species);
  if (!config)
    throw std::runtime_error("missing fixture configuration");
  auto *track = (new G4Molecule(config))->BuildTrack(time, pos);
  track->SetTrackID(id);
  track->SetTrackStatus(fAlive);
  ancestry.emplace(id, Ancestry{1u << packet, time});
  SourceContacts::activation[id] = time;
  SourceContacts::RegisterPrivateSource(id, time);
  G4ITTrackHolder::Instance()->Push(track);
}
} // namespace
int main(int argc, char **argv)
{
  try
  {
    if (argc != 3 && argc != 4 && argc != 5)
      throw std::runtime_error("macro and fixture required");
    SourceContacts::enabled = argc == 5 && std::string(argv[4]) == "patched";
    SourceContacts::ResetPrivateHistory();
    const std::string fixture = argv[2];
    if(fixture=="ancestry") {
      const auto mixed=MergeAncestry({1,1},{2,2});
      const auto followup=MergeAncestry(mixed,{1,3});
      if(mixed.mask!=3 || mixed.activation!=2 || followup.mask!=3 || followup.activation!=3 || !(mixed.mask & 1) || (mixed.mask & 4)) throw std::runtime_error("mixed ancestry followup failed");
      std::cout<<"ANCESTRY_PASS\n"; return 0;
    }
    checkBulk = fixture == "bulk";
    checkScore = fixture == "two";
    if (fixture != "simultaneous" && fixture != "delayed" && fixture != "shifted" && fixture != "newold" && fixture != "interrupted" && fixture != "empty" && fixture != "union" &&
        fixture != "nonreacting" && fixture != "bulk" && fixture != "two" && fixture != "nearend" && fixture != "descendant")
      throw std::runtime_error("unknown fixture");
    std::unique_ptr<G4RunManager> manager(
        G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new DetectorConstruction);
    manager->SetUserInitialization(new PhysicsList);
    manager->SetUserInitialization(new ActionInitialization);
    std::ifstream macro(argv[1]);
    if (!macro)
      throw std::runtime_error("missing macro");
    std::string line;
    while (std::getline(macro, line))
      if (!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line))
        throw std::runtime_error("macro failed");
    G4Scheduler::Instance()->SetUserAction(new InjectionAction);
    G4Scheduler::Instance()->SetEndTime(20 * ps);
    // Distant O2 establishes an older live bath and the 1 ps scheduler origin.
    Add("O2", (fixture == "shifted" ? 10 : 1) * ps, {100 * um, 0, 0}, 1000001, 0);
    const double activation = (fixture == "simultaneous" || fixture == "union" || fixture == "two" ? 1 : fixture == "newold" ? 1.001 : fixture == "nearend" ? 19.999 : 10) * ps;
    const std::string species = fixture == "nonreacting" || fixture == "bulk" ? "O2" : "e_aq";
    if (fixture != "empty") {
      Add(species, fixture == "newold" ? 1*ps : activation, {0, 0, 0}, 1000002, 1);
      Add(species, activation, {0.01 * nm, 0, 0}, 1000003, fixture == "union" ? 1 : 2);
    }
    if (fixture == "interrupted" || fixture == "descendant") {
      const G4MolecularConfiguration* hole = nullptr;
      for (int i = 0; i < G4MolecularConfiguration::GetNumberOfSpecies(); ++i) {
        auto* config = G4MolecularConfiguration::GetMolecularConfiguration(i);
        if (config->GetDefinition()->GetParticleName() == "H2O" && config->GetCharge() == 1) {
          hole = config;
          break;
        }
      }
      if (!hole) throw std::runtime_error("missing ionized-water fixture");
      auto* ion = (new G4Molecule(hole))->BuildTrack(activation, {1*um,0,0});
      ion->SetTrackID(1000004);
      ion->SetTrackStatus(fStopButAlive);
      ancestry.emplace(1000004, Ancestry{3,activation});
      SourceContacts::activation[1000004]=activation;
      SourceContacts::RegisterPrivateSource(1000004,activation);
      G4ITTrackHolder::Instance()->Push(ion);
      Add("e_aq", activation, {1*um+0.1*nm,0,0},1000005,3);
    }
    if (fixture == "descendant")
      Add(std::string("\xC2\xB0")+"OH",1*ps,{1*um+0.01*nm,0,0},1000006,4);
    if (fixture == "two") {
      Add("e_aq",10*ps,{1*um,0,0},1000004,3);
      Add("e_aq",10*ps,{1*um+0.01*nm,0,0},1000005,4);
    }
    auto *electron = G4MolecularConfiguration::GetMolecularConfiguration("e_aq");
    auto *reaction =
        G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(electron, electron);
    if (!reaction || reaction->GetProbability() != 1 ||
        reaction->GetEffectiveReactionRadius() <= 0.01 * nm)
      throw std::runtime_error("contact fixture assumptions invalid");
    std::cout << "CONTACT radius_nm " << reaction->GetEffectiveReactionRadius() / nm
              << " separation_nm 0.01 probability " << reaction->GetProbability() << std::endl;
    G4Random::setTheSeed(argc >= 4 ? std::stol(argv[3]) : 1729);
    G4DNAChemistryManager::Instance()->UseAsStandalone(true);
    G4DNAChemistryManager::Instance()->Run();
    std::cout << "CONTACT_AUDIT newNew " << SourceContacts::newNew << " newOld " << SourceContacts::newOld << " oldOld " << SourceContacts::oldOld << std::endl;
    if (premature)
      throw std::runtime_error("premature activation");
    if (fixture == "empty") {
      if (totalReactions != 0 || survivors != 1)
        throw std::runtime_error("empty source changed bath");
    }
    else if (fixture == "union") {
      if (totalReactions != 1 || crossReactions != 0 || survivors != 4)
        throw std::runtime_error("simultaneous union failed");
    }
    else if (fixture == "nonreacting" || fixture == "bulk")
    {
      if (crossReactions != 0 || survivors != 3)
        throw std::runtime_error("population accounting failed");
      if (fixture == "bulk" && bulkChecks < 3)
        throw std::runtime_error("bulk continuity checkpoints missing");
    }
    else if (fixture == "two") {
      if (crossReactions != 2 || totalReactions != 2)
        throw std::runtime_error("reaction score did not accumulate across arrivals");
    }
    else if (fixture == "descendant") {
      if (crossReactions < 1) throw std::runtime_error("primary contact lost in descendant fixture");
      std::cout << "DESCENDANT_CROSS " << descendantCrossReactions << '\n';
    }
    else if (crossReactions != 1)
      throw std::runtime_error("contact pair failed to react across packets");
    std::cout << "INJECTION_CONTROL_PASS " << fixture << '\n';
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << "INJECTION_CONTROL_FAIL " << e.what() << '\n';
    return 2;
  }
}

