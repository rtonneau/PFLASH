// Fixed electron-hole fixture for private-library diagnostics.
#define main ExistingContactMain
#include "InjectionReference.cc"
#undef main
#include "G4ChemicalMoleculeFinder.hh"
#include "G4MoleculeDefinition.hh"
#include <iomanip>

int main(int argc, char** argv)
{
  try {
    if (argc != 2) throw std::runtime_error("macro required");
    std::unique_ptr<G4RunManager> manager(
      G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new DetectorConstruction);
    manager->SetUserInitialization(new PhysicsList);
    manager->SetUserInitialization(new ActionInitialization);
    std::ifstream macro(argv[1]);
    if (!macro) throw std::runtime_error("missing macro");
    std::string line;
    while (std::getline(macro, line))
      if (!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line))
        throw std::runtime_error("macro failed");
    G4cout << std::setprecision(17);
    for (const auto* reaction : G4DNAMolecularReactionTable::GetReactionTable()->GetVectorOfReactionData()) {
      G4cout << "TABLE " << reaction->GetReactionID() << " "
             << reaction->GetReactant1()->GetName() << " "
             << reaction->GetReactant2()->GetName() << " "
             << reaction->GetObservedReactionRateConstant() << " "
             << reaction->GetEffectiveReactionRadius() << " "
             << reaction->GetProbability();
      for (int i = 0; i < reaction->GetNbProducts(); ++i)
        G4cout << " " << reaction->GetProduct(i)->GetName();
      G4cout << G4endl;
    }
    const G4MolecularConfiguration* hole = nullptr;
    for (int i = 0; i < G4MolecularConfiguration::GetNumberOfSpecies(); ++i) {
      auto* config = G4MolecularConfiguration::GetMolecularConfiguration(i);
      if (config->GetDefinition()->GetParticleName() == "H2O" && config->GetCharge() == 1) {
        hole = config;
        break;
      }
    }
    if (!hole) throw std::runtime_error("missing ionized water configuration");
    auto* track = (new G4Molecule(hole))->BuildTrack(1*ps, {0,0,0});
    track->SetTrackID(1000001);
    track->SetTrackStatus(fStopButAlive);
    G4ITTrackHolder::Instance()->Push(track);
    Add("e_aq", 1*ps, {0.1*nm,0,0}, 1000002, 1);
    G4Scheduler::Instance()->SetUserAction(new InjectionAction);
    G4Scheduler::Instance()->SetEndTime(2*ps);
    G4Random::setTheSeed(81001);
    G4cout << "FINDER_EXE " << G4ChemicalMoleculeFinder::Instance()
           << " used " << G4ChemicalMoleculeFinder::Instance()->IsOctreeUsed()
           << " built " << G4ChemicalMoleculeFinder::Instance()->IsOctreeBuilt()
           << " hole " << hole->GetName() << G4endl;
    G4Random::getTheEngine()->put(G4cout);
    G4DNAChemistryManager::Instance()->UseAsStandalone(true);
    G4DNAChemistryManager::Instance()->Run();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
