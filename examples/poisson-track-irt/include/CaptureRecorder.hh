// CaptureRecorder: per-event state of `capture` (Serial run only).
//
// One event = one primary = one PrecursorPacket. The user actions forward to it:
//   PrimaryGeneratorAction  BeginEvent    reseed with the declared physics seed,
//                                         record the primary (particle, energy, origin)
//   SteppingAction          AddDeposit    every step's total energy deposit
//   TrackingAction          EndTrack      kinetic energy of tracks leaving the world
//   CaptureTimeStepAction   CaptureAtChemistryStart
//                                         copy the molecules waiting in the
//                                         G4ITTrackHolder delayed lists (all at the
//                                         1 ps precursor age), then stop the chemistry
//   EventAction             EndEvent      build, validate, write and re-read the packet
//
// Capture runs inside G4DNAChemistryManager::Run (G4Scheduler::Process), so no
// Geant4 source is instrumented. The chemistry is stopped before its first step:
// no chemistry random number is drawn and nothing reacts.
//
// Errors never throw through the Geant4 kernel: the first one is stored
// (Failure()), the chemistry is stopped and the run is aborted.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "CaptureLibrary.hh"
#include "PrecursorPacket.hh"

#include "G4ThreeVector.hh"
#include "globals.hh"

class G4Event;
class G4ParticleGun;
class G4Track;

namespace ptirt
{

struct CaptureSettings
{
  std::filesystem::path outputDirectory;  // must exist; packets are written here
  long long firstPhysicsSeed = 1;         // event i uses firstPhysicsSeed + i
  PacketLimits limits;
};

// Per-packet values that the packet file itself does not hold.
struct CaptureProvenance
{
  std::array<double, 3> primaryOriginNm{};  // gun position (packet coordinates are relative to it)
  std::array<double, 3> primaryDirection{};
};

class CaptureRecorder
{
 public:
  explicit CaptureRecorder(CaptureSettings settings);

  // The half side of the capture box, known after /run/initialize (the macro
  // may change it). Must be set before the first event.
  void SetCaptureHalfSide(G4double halfSide) { fHalfSide = halfSide; }

  // Empty if the gun can be used: one particle, positive energy, unit
  // direction, position strictly inside the capture box.
  // (Non-const gun: G4VPrimaryGenerator::GetParticlePosition is not const.)
  std::string DescribeGunProblem(G4ParticleGun& gun) const;

  void BeginEvent(G4int eventId, G4ParticleGun& gun);
  void AddDeposit(G4double energyDeposit);
  void EndTrack(const G4Track& track);
  void CaptureAtChemistryStart();
  void EndEvent(const G4Event& event);

  const std::string& Failure() const { return fFailure; }
  const std::vector<LibraryEntry>& Entries() const { return fEntries; }
  const std::vector<CaptureProvenance>& Provenance() const { return fProvenance; }
  G4double GetCaptureHalfSide() const { return fHalfSide; }

 private:
  void Fail(const std::string& message);

  CaptureSettings fSettings;
  G4double fHalfSide = 0;
  std::string fFailure;
  std::vector<LibraryEntry> fEntries;
  std::vector<CaptureProvenance> fProvenance;

  // Current event.
  G4int fEventId = -1;
  long long fPhysicsSeed = 0;
  G4String fPrimaryParticle;
  G4double fPrimaryEnergy = 0;
  G4ThreeVector fOrigin;
  G4ThreeVector fDirection;
  G4double fDeposit = 0;
  G4double fEscaped = 0;
  bool fChemistryStarted = false;
  std::vector<PacketMolecule> fMolecules;
};

}  // namespace ptirt
