#include "ProbeHooks.hh"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4ITTrackHolder.hh"
#include "G4Molecule.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Probe {
namespace {
std::string mode;
std::filesystem::path directory;
thread_local std::ofstream trace;
thread_local int eventId = -1;
thread_local bool captured = false;
constexpr double particleCutoff = 7.123456789 * CLHEP::ps;
constexpr double mesoCutoff = 7.123456789 * CLHEP::ns;

// Full engine state, no draws. No address values or worker ids enter the trace.
void Rng(std::ostream &out)
{
  out << "rng " << G4Random::getTheEngine()->name();
  for (auto word : G4Random::getTheEngine()->put()) out << ' ' << word;
  out << '\n';
}
void Bulk(std::ostream &out)
{
  auto *material = dynamic_cast<G4DNAScavengerMaterial *>(
    G4Scheduler::Instance()->GetScavengerMaterial());
  if (!material) throw std::runtime_error("bulk scavenger missing");
  std::map<std::string, double> counts;
  for (const auto *conf : material->GetScavengerList())
    counts[conf->GetName()] = material->GetNumberMoleculePerVolumeUnitForMaterialConf(conf);
  for (const auto &[name, count] : counts) out << "bulk " << name << ' ' << count << '\n';
}
void Tracks(std::ostream &out)
{
  std::vector<const G4Track *> tracks;
  for (auto *track : *G4ITTrackHolder::Instance()->GetMainList()) tracks.push_back(track);
  std::sort(tracks.begin(), tracks.end(), [](auto *a, auto *b) { return a->GetTrackID() < b->GetTrackID(); });
  for (const auto *track : tracks) {
    const auto &p = track->GetPosition();
    out << "track " << track->GetTrackID() << ' ' << GetMolecule(track)->GetName()
        << ' ' << track->GetTrackStatus() << ' ' << track->GetGlobalTime()
        << ' ' << p.x() << ' ' << p.y() << ' ' << p.z() << '\n';
  }
}
void MeshState(const G4DNAMesh &mesh, std::ostream &out)
{
  out << "resolution " << mesh.GetResolution() << '\n';
  // Preserve voxel traversal; canonicalize species names for observation only.
  for (auto it = mesh.const_begin(); it != mesh.const_end(); ++it) {
    std::map<std::string, std::size_t> species;
    for (const auto &[conf, count] : std::get<2>(*it)) species[conf->GetName()] = count;
    for (const auto &[name, count] : species)
      out << "cell " << std::get<0>(*it) << ' ' << name << ' ' << count << '\n';
  }
}
void Capture(double requested, double actual, std::size_t pending, bool pendingKnown)
{
  std::ofstream evidence(directory / ("event-" + std::to_string(eventId) + ".capture"));
  evidence.exceptions(std::ios::badbit | std::ios::failbit);
  evidence << std::setprecision(17)
           << "requested " << requested << "\nactual " << actual
           << "\nexact " << (requested == actual) << "\npendingKnown " << pendingKnown
           << "\npending " << pending << "\nrestorable 0\n";
  Rng(evidence);
  Bulk(evidence);
  Tracks(evidence);
  captured = true;
}
}
void Configure(const std::string &requestedMode, const std::filesystem::path &path)
{
  if (requestedMode != "oracle" && requestedMode != "particle" && requestedMode != "meso")
    throw std::runtime_error("unknown probe mode");
  mode = requestedMode;
  directory = path;
  std::filesystem::create_directories(directory);
}
void BeginEvent(int id)
{
  eventId = id;
  captured = false;
  trace.close();
  trace.clear();
  trace.open(directory / ("event-" + std::to_string(id) + ".trace"));
  trace.exceptions(std::ios::badbit | std::ios::failbit);
  trace << std::setprecision(17);
  // Fixture streams are derived only from the stable event id, before physics.
  // This experiment has one run and a bounded, small event range.
  G4Random::setTheSeed(12345L + 104729L * id);
  trace << "event " << id << '\n';
  Rng(trace);
}
void StartChemistry()
{
  trace << "chemistry-start\n";
  Rng(trace);
  if (mode == "particle") G4Scheduler::Instance()->AddWatchedTime(particleCutoff);
}
void Particle(const char *phase)
{
  auto *scheduler = G4Scheduler::Instance();
  trace << "particle " << phase << ' ' << scheduler->GetGlobalTime() << ' '
        << scheduler->GetTimeStep() << '\n';
  Tracks(trace);
  Rng(trace);
  if (mode == "particle" && std::string(phase) == "post" && scheduler->GetGlobalTime() >= particleCutoff) {
    // The Windows shared library does not export the singleton data used by
    // G4ITReactionSet::Instance(). Do not create a second, empty singleton or
    // misreport unavailable queue state as a measured zero.
    Capture(particleCutoff, scheduler->GetGlobalTime(), 0, false);
    scheduler->Stop();
  }
}
void Reaction(const G4Track &a, const G4Track &b)
{
  trace << "reaction " << G4Scheduler::Instance()->GetGlobalTime() << ' '
        << a.GetTrackID() << ' ' << GetMolecule(a)->GetName() << ' '
        << b.GetTrackID() << ' ' << GetMolecule(b)->GetName() << '\n';
}
void Mesh(const G4DNAMesh &mesh, double time)
{
  trace << "mesh " << time << '\n';
  MeshState(mesh, trace);
  Bulk(trace);
  Rng(trace);
}
void BeforeMeso(G4DNAEventScheduler &scheduler)
{
  trace << "meso-start " << scheduler.GetStartTime() << " original-end " << scheduler.GetEndTime() << '\n';
  if (mode == "meso") scheduler.SetEndTime(mesoCutoff);
}
void AfterMeso(G4DNAEventScheduler &scheduler)
{
  trace << "meso-return " << scheduler.GetGlobalTime() << '\n';
  if (mode == "meso") {
    Capture(mesoCutoff, scheduler.GetGlobalTime(), 0, false);
    // GetMesh is observable; the private pending event set is not exported.
    std::ofstream meshFile(directory / ("event-" + std::to_string(eventId) + ".mesh"));
    meshFile.exceptions(std::ios::badbit | std::ios::failbit);
    meshFile << std::setprecision(17);
    if (scheduler.GetMesh()) MeshState(*scheduler.GetMesh(), meshFile);
    G4Scheduler::Instance()->Stop();
  }
}
void EndChemistry()
{
  trace << "chemistry-end " << G4Scheduler::Instance()->GetGlobalTime() << " captured " << captured << '\n';
  Rng(trace);
  Bulk(trace);
  trace.flush();
}
}
