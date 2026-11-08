// Isolated feasibility diagnostic. Does not publish production results or
// claim that observation files are restorable checkpoints.
#include "ProbeHooks.hh"
#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "Randomize.hh"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main(int argc, char **argv)
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  try {
    if (argc != 6) throw std::runtime_error("usage: probe macro directory mode workers events");
    const int workers = std::stoi(argv[4]);
    const int events = std::stoi(argv[5]);
    if (workers < 0 || events < 1) throw std::runtime_error("invalid workers/events");
    Probe::Configure(argv[3], argv[2]);
    DnaLogger::SetLevel(DnaLogger::Level::Quiet);
    G4String error;
    if (!OutputDir::Configure(argv[2], error)) throw std::runtime_error(error);
    G4Random::setTheSeed(12345);
    std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(
      workers ? G4RunManagerType::MT : G4RunManagerType::Serial));
    if (workers) manager->SetNumberOfThreads(workers);
    OutputFormatsMessenger outputFormats;
    manager->SetUserInitialization(new DetectorConstruction());
    manager->SetUserInitialization(new PhysicsList());
    manager->SetUserInitialization(new ActionInitialization());
    auto *ui = G4UImanager::GetUIpointer();
    // Apply lines individually: /control/execute can hide a nested command error.
    std::ifstream macro(argv[1]);
    if (!macro) throw std::runtime_error("cannot open macro");
    std::string line;
    while (std::getline(macro, line)) {
      if (line.empty() || line[0] == '#') continue;
      if (ui->ApplyCommand(line) != 0) throw std::runtime_error("macro command failed: " + line);
    }
    manager->BeamOn(events);
    std::cout << "PROBE_PROCESS_DONE events=" << events << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "PROBE_ERROR " << error.what() << '\n';
    return 2;
  }
}
