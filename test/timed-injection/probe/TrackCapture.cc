#include "G4Box.hh"
#include "G4DNAChemistryManager.hh"
#include "G4ElectronOccupancy.hh"
#include "G4Event.hh"
#include "G4ITTrackHolder.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeDefinition.hh"
#include "G4Navigator.hh"
#include "G4ParticleGun.hh"
#include "G4RunManagerFactory.hh"
#include "G4Scheduler.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4TransportationManager.hh"
#include "G4UImanager.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"
#include "TrackPacket.hh"
#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "scoring/OutputFormatsMessenger.hh"
#include "scoring/PrimaryKiller.hh"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
namespace
{
Packets::Packet packet;
std::string mode, prefix;
long chemistrySeed = 1;
std::mt19937_64 sourceRandom;
int completed = 0;
bool captured = false;
std::string eventFailure;
double escapedEnergy = 0;
void Require(bool b, const char *message)
{
  if (!b)
    throw std::runtime_error(message);
}
std::array<double, 3> Array(const G4ThreeVector &v) { return {v.x(), v.y(), v.z()}; }
G4ThreeVector Vector(const std::array<double, 3> &v) { return {v[0], v[1], v[2]}; }
Packets::Molecule State(const G4Track *t)
{
  auto *c = GetMolecule(t)->GetMolecularConfiguration();
  Packets::Molecule m;
  m.definition = c->GetDefinition()->GetParticleName();
  m.charge = c->GetCharge();
  if (auto *occ = c->GetElectronOccupancy())
    for (int i = 0; i < occ->GetSizeOfOrbit(); ++i)
      m.occupancy.push_back(occ->GetOccupancy(i));
  m.status = t->GetTrackStatus();
  m.position = Array(t->GetPosition());
  m.direction = Array(t->GetMomentumDirection());
  m.globalTime = t->GetGlobalTime();
  m.localTime = t->GetLocalTime();
  m.properTime = t->GetProperTime();
  m.decayTime = c->GetDecayTime();
  m.kineticEnergy = t->GetKineticEnergy();
  return m;
}
const G4MolecularConfiguration *Resolve(const Packets::Molecule &m)
{
  for (int i = 0; i < G4MolecularConfiguration::GetNumberOfSpecies(); ++i)
  {
    auto *c = G4MolecularConfiguration::GetMolecularConfiguration(i);
    if (c->GetDefinition()->GetParticleName() != m.definition || c->GetCharge() != m.charge ||
        c->GetDecayTime() != m.decayTime)
      continue;
    std::vector<int> occ;
    if (auto *e = c->GetElectronOccupancy())
      for (int j = 0; j < e->GetSizeOfOrbit(); ++j)
        occ.push_back(e->GetOccupancy(j));
    if (occ == m.occupancy)
      return c;
  }
  throw std::runtime_error("unknown molecular configuration");
}
} // namespace
void ProbeTruncated() { eventFailure = "truncated track"; }
void ProbePrimary(G4ParticleGun *gun)
{
  eventFailure.clear();
  escapedEnergy = 0;
  packet.energyDeposit = packet.secondaryDeposit = 0;
  packet.molecules.clear();
  captured = false;
  std::uniform_real_distribution<double> uniform(-packet.halfSide, packet.halfSide);
  packet.entry = {uniform(sourceRandom), uniform(sourceRandom), -packet.halfSide};
  gun->SetParticlePosition(Vector(packet.entry));
  gun->SetParticleMomentumDirection({0, 0, 1});
  gun->SetParticleEnergy(500 * keV);
}
void ProbeTrack(const G4Track *track)
{
  if (track->GetNextVolume() == nullptr)
    escapedEnergy += track->GetKineticEnergy();
}
void ProbeStep(const G4Step *step)
{

  const auto *volume = step->GetPreStepPoint()->GetPhysicalVolume();
  if (volume && volume == G4TransportationManager::GetTransportationManager()
                              ->GetNavigatorForTracking()
                              ->GetWorldVolume())
  {
    packet.energyDeposit += step->GetTotalEnergyDeposit();
    if (step->GetTrack()->GetParentID() != 0)
      packet.secondaryDeposit += step->GetTotalEnergyDeposit();
  }
}
void CapturePrecursors()
{
  if (mode != "import")
  {
    const auto *event = G4RunManager::GetRunManager()->GetCurrentEvent();
    if (event != nullptr && event->IsAborted())
    {
      eventFailure = "aborted event";
      G4Scheduler::Instance()->Stop();
      return;
    }
    if (!eventFailure.empty() || PrimaryKiller::PrimaryKilledThisEvent())
    {
      eventFailure = "truncated track";
      G4Scheduler::Instance()->Stop();
      return;
    }
    for (auto &[time, lists] : G4ITTrackHolder::Instance()->GetDelayedLists())
      for (auto &[key, list] : lists)
        for (auto *t : *list)
        {
          Require(time == t->GetGlobalTime(), "delayed time mismatch");
          packet.molecules.push_back(State(t));
        }
    Packets::Validate(packet);
    std::ofstream out(prefix + ".packet");
    Packets::Write(out, packet);
    captured = true;
  }
  G4Random::setTheSeed(chemistrySeed);
  if (mode == "capture")
    G4Scheduler::Instance()->Stop();
}
void ProbeEndChemistry()
{
  std::map<std::string, int> counts;
  for (auto *t : *G4ITTrackHolder::Instance()->GetMainList())
    if (t->GetTrackStatus() != fStopAndKill)
      ++counts[GetMolecule(t)->GetMolecularConfiguration()->GetName()];
  std::ofstream out(prefix + ".chemistry");
  out << std::setprecision(17) << G4Scheduler::Instance()->GetGlobalTime() << '\n';
  for (auto &[name, n] : counts)
    out << name << ' ' << n << '\n';
  Require(bool(out), "chemistry output failed");
}
void ProbeEvent(const G4Event *event)
{
  if (event->IsAborted())
    eventFailure = "aborted event";
  if (PrimaryKiller::PrimaryKilledThisEvent() && eventFailure.empty())
    eventFailure = "truncated event";
  if (!eventFailure.empty())
  {
    ++completed;
    return;
  }
  if (std::abs(packet.energyDeposit + escapedEnergy - 500 * keV) > 1e-8 * MeV)
    eventFailure = "energy balance failed";
  std::ofstream balance(prefix + ".energy");
  balance << std::setprecision(17) << packet.energyDeposit << " " << escapedEnergy << " "
          << packet.energyDeposit + escapedEnergy << "\n";
  // Empty incident histories are retained even when chemistry has no tracks.
  if (!captured)
  {
    std::ofstream out(prefix + ".packet");
    Packets::Write(out, packet);
  }
  ++completed;
}
int main(int argc, char **argv)
{
  try
  {
    Require(argc == 7, "usage: TrackCapture macro capture|direct|import prefix physicsSeed "
                       "sourceSeed chemistrySeed");
    mode = argv[2];
    prefix = argv[3];
    Require(mode == "capture" || mode == "direct" || mode == "import", "invalid mode");
    sourceRandom.seed(std::stoull(argv[5]));
    chemistrySeed = std::stol(argv[6]);
    G4Random::setTheSeed(std::stol(argv[4]));
    std::unique_ptr<G4RunManager> manager(
        G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    OutputFormatsMessenger formats;
    manager->SetUserInitialization(new DetectorConstruction);
    manager->SetUserInitialization(new PhysicsList);
    manager->SetUserInitialization(new ActionInitialization);
    std::ifstream macro(argv[1]);
    Require(bool(macro), "missing macro");
    std::string line, macroIdentity;
    while (std::getline(macro, line))
    {
      macroIdentity += line + "\n";
      if (!line.empty())
        Require(G4UImanager::GetUIpointer()->ApplyCommand(line) == 0, "invalid macro command");
    }
    auto *world = G4TransportationManager::GetTransportationManager()
                      ->GetNavigatorForTracking()
                      ->GetWorldVolume()
                      ->GetLogicalVolume();
    auto *box = dynamic_cast<G4Box *>(world->GetSolid());
    Require(box && box->GetXHalfLength() == 250 * um && box->GetYHalfLength() == 250 * um &&
                box->GetZHalfLength() == 250 * um,
            "incompatible cube");
    packet.halfSide = box->GetXHalfLength();
    packet.massKg = world->GetSolid()->GetCubicVolume() * world->GetMaterial()->GetDensity() / kg;
    packet.compatibility = std::string(PACKET_COMPATIBILITY) + "|" + macroIdentity;
    if (mode == "import")
    {
      const auto expected = packet;
      std::ifstream in(prefix + ".packet");
      packet = Packets::Read(in, expected.compatibility);
      Require(packet.massKg == expected.massKg && packet.halfSide == expected.halfSide,
              "incompatible geometry/material");
      // Resolve every configuration before constructing any track (transactional validation).
      std::vector<const G4MolecularConfiguration *> configs;
      for (const auto &m : packet.molecules)
        configs.push_back(Resolve(m));
      int freshID = 1000000;
      for (std::size_t i = 0; i < packet.molecules.size(); ++i)
      {
        const auto &m = packet.molecules[i];
        auto *molecule = new G4Molecule(configs[i]);
        auto *t = molecule->BuildTrack(m.globalTime, Vector(m.position));
        t->SetTrackID(++freshID);
        t->SetParentID(0);
        t->SetTrackStatus(static_cast<G4TrackStatus>(m.status));
        t->SetLocalTime(m.localTime);
        t->SetProperTime(m.properTime);
        t->SetMomentumDirection(Vector(m.direction));
        t->SetKineticEnergy(m.kineticEnergy);
        Require(State(t) == m, "import state mismatch");
        G4ITTrackHolder::Instance()->Push(t);
        Require(t->GetTrackID() == freshID, "fresh ID overwritten");
      }
      std::ofstream out(prefix + ".roundtrip");
      Packets::Write(out, packet);
      out.close();
      G4DNAChemistryManager::Instance()->UseAsStandalone(true);
      G4DNAChemistryManager::Instance()->Run();
      std::cout << "IMPORT_VALIDATED " << packet.molecules.size() << " fresh instances\n";
    }
    else
    {
      manager->BeamOn(1);
      Require(eventFailure.empty(), eventFailure.c_str());
      Require(completed == 1, "incomplete event");
      std::cout << std::setprecision(17) << "PACKET_VALIDATED " << packet.molecules.size()
                << " Edep " << packet.energyDeposit / eV << " secondary "
                << packet.secondaryDeposit / eV << " massKg " << packet.massKg << '\n';
    }
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << "PACKET_ERROR " << e.what() << '\n';
    return 2;
  }
}
