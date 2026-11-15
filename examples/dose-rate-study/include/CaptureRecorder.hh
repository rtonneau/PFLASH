// CaptureRecorder: per-event state of `capture` (Serial run only).
//
// One event = one primary crossing the capture box = one PrecursorPacket. The
// user actions forward to it:
//   PrimaryGeneratorAction  BeginEvent    reseed with the declared physics seed, place
//                                         the primary 1 nm inside the -z world face at a
//                                         uniformly random (x, y) over the box face, aim
//                                         it along +z, record it
//   SteppingAction          AddDeposit    every step's total energy deposit, split
//                                         into inside / outside the capture box by the
//                                         pre-step volume
//   TrackingAction          EndTrack      kinetic energy of tracks leaving the world
//   CaptureTimeStepAction   CaptureAtChemistryStart
//                                         copy the molecules waiting in the
//                                         G4ITTrackHolder delayed lists (all at the
//                                         1 ps precursor age) that lie strictly inside
//                                         the capture box, then stop the chemistry
//   EventAction             EndEvent      build, validate, write and re-read the packet
//
// Packet energies: energyDeposit = the energy deposited inside the box (the
// packet's dose, wherever the depositing particle was born, so delta rays from
// upstream count when they deposit inside); escapedEnergy = the rest of the
// primary energy (deposited in the surrounding water + carried out of the
// world), so the packet's energy balance stays a real check of the transport.
// Molecule positions are relative to the box centre (the world origin).
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
class G4Step;
class G4Track;
class G4VPhysicalVolume;

namespace drs
{

struct CaptureSettings
{
  std::filesystem::path outputDirectory;  // must exist; packets are written here
  long long firstPhysicsSeed = 1;         // event i uses firstPhysicsSeed + i (i counts every event)
  std::size_t maxEmpty = 0;               // events without any precursor molecule that may be
                                          // skipped (recorded, no packet); one more is an error
  PacketLimits limits;
};

// Per-packet values that the packet file itself does not hold.
struct CaptureProvenance
{
  std::array<double, 3> primaryOriginNm{};  // gun position, world coordinates (box centre = 0)
  std::array<double, 3> primaryDirection{};
  double outsideDepositEv = 0;       // deposited in the water around the box
  double worldEscapeEv = 0;          // kinetic energy carried out of the world
  std::size_t keptMolecules = 0;     // inside the box at 1 ps (the packet)
  std::size_t droppedMolecules = 0;  // outside the box at 1 ps
};

class CaptureRecorder
{
 public:
  explicit CaptureRecorder(CaptureSettings settings);

  // The capture geometry, known after /run/initialize (the macro may change
  // the sizes). Must be set before the first event. boxVolume is the capture
  // box placement (deposits in it are the packet's dose).
  void SetGeometry(G4double boxHalfSide, G4double worldHalfSide, const G4VPhysicalVolume* boxVolume);

  // Empty if the gun can be used: one particle, positive energy, unit
  // direction, time 0, position strictly inside the world.
  // (Non-const gun: G4VPrimaryGenerator::GetParticlePosition is not const.)
  std::string DescribeGunProblem(G4ParticleGun& gun) const;

  void BeginEvent(G4int eventId, G4ParticleGun& gun);
  void AddDeposit(const G4Step& step);
  void EndTrack(const G4Track& track);
  void CaptureAtChemistryStart();
  void EndEvent(const G4Event& event);

  const std::string& Failure() const { return fFailure; }
  const std::vector<LibraryEntry>& Entries() const { return fEntries; }
  const std::vector<CaptureProvenance>& Provenance() const { return fProvenance; }
  // Skipped events (no precursor molecule in the capture box): seed and deposit.
  struct SkippedEvent
  {
    long long physicsSeed = 0;
    double energyDepositEv = 0;
  };
  const std::vector<SkippedEvent>& Skipped() const { return fSkipped; }
  G4double GetCaptureHalfSide() const { return fHalfSide; }
  G4double GetWorldHalfSide() const { return fWorldHalfSide; }

 private:
  void Fail(const std::string& message);

  CaptureSettings fSettings;
  G4double fHalfSide = 0;  // capture box
  G4double fWorldHalfSide = 0;
  const G4VPhysicalVolume* fBoxVolume = nullptr;
  std::string fFailure;
  std::vector<LibraryEntry> fEntries;
  std::vector<CaptureProvenance> fProvenance;
  std::vector<SkippedEvent> fSkipped;

  // Current event.
  G4int fEventId = -1;
  long long fPhysicsSeed = 0;
  long long fEventsBegun = 0;
  G4String fPrimaryParticle;
  G4double fPrimaryEnergy = 0;
  G4ThreeVector fOrigin;
  G4ThreeVector fDirection;
  G4double fDeposit = 0;         // inside the capture box
  G4double fOutsideDeposit = 0;  // in the world outside the box
  G4double fEscaped = 0;         // carried out of the world
  std::size_t fDropped = 0;      // molecules outside the box at 1 ps
  bool fChemistryStarted = false;
  std::vector<PacketMolecule> fMolecules;
};

}  // namespace drs
