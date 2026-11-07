#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4ITTrackHolder.hh"
#include "G4Molecule.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <iomanip>
namespace { int captured = 0; }
void CapturePrecursors()
{
  std::ofstream out("precursors.tsv");
  out << std::setprecision(17);
  for (auto& [time, lists] : G4ITTrackHolder::Instance()->GetDelayedLists()) {
    for (auto& [key, list] : lists) {
      for (auto* track : *list) {
        auto* molecule = GetMolecule(track);
        auto* configuration = molecule->GetMolecularConfiguration();
        // Test whether a fresh molecule made from the full configuration retains
        // the precursor decay clock. This does not certify file import.
        G4Molecule rebuilt(configuration);
        out << configuration->GetName() << '\t' << track->GetTrackStatus()
            << '\t' << time / ps << '\t' << molecule->GetDecayTime()/ps
            << '\t' << rebuilt.GetDecayTime()/ps << '\n';
        if (rebuilt.GetDecayTime() != molecule->GetDecayTime())
          throw std::runtime_error("configuration reconstruction loses decay time");
        ++captured;
      }
    }
  }
  if (!captured) throw std::runtime_error("no precursor tracks observed");
  std::cout << "CAPTURED_PRECURSORS " << captured << '\n';
  G4Scheduler::Instance()->Stop();
}
int main(int argc, char** argv)
{
  try {
    if (argc != 2) throw std::runtime_error("macro required");
    std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new DetectorConstruction);
    manager->SetUserInitialization(new PhysicsList);
    manager->SetUserInitialization(new ActionInitialization);
    std::ifstream macro(argv[1]);
    if (!macro) throw std::runtime_error("missing macro");
    std::string line;
    while (std::getline(macro,line))
      if (!line.empty() && G4UImanager::GetUIpointer()->ApplyCommand(line))
        throw std::runtime_error("invalid macro command: " + line);
    manager->BeamOn(1);
    return captured ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 2;
  }
}

