#pragma once
#include <filesystem>
#include <string>
class G4DNAMesh;
class G4DNAEventScheduler;
class G4Track;
namespace Probe {
void Configure(const std::string &mode, const std::filesystem::path &directory);
void BeginEvent(int eventId);
void StartChemistry();
void Particle(const char *phase);
void Reaction(const G4Track &a, const G4Track &b);
void Mesh(const G4DNAMesh &mesh, double time);
void BeforeMeso(G4DNAEventScheduler &scheduler);
void AfterMeso(G4DNAEventScheduler &scheduler);
void EndChemistry();
}
