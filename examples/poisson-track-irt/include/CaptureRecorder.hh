// CaptureRecorder: per-event state of `capture` (Serial run only).
//
// One event = one primary = one PrecursorPacket. The user actions forward to it:
//   PrimaryGeneratorAction  BeginEvent    reseed with the declared physics seed,
//                                         record the primary (particle, energy, origin)
//   SteppingAction          AddDeposit    every step's total energy deposit
//   TrackingAction          EndTrack      kinetic energy of tracks leaving the world
//   CaptureTimeStepAction   StartChemistry
//                                         the chemistry is about to start: digest the
//                                         molecules waiting in the G4ITTrackHolder
//                                         delayed lists (all at the 1 ps precursor age)
//                           BeforeTimeStep
//                                         before every chemistry step (stock only)
//                           OnReaction    every reaction (stock only, before the capture)
//   EventAction             EndEvent      build, validate, write and re-read the packet
//
// Capture runs inside G4DNAChemistryManager::Run (G4Scheduler::Process), so no
// Geant4 source is instrumented. Where the molecules are read depends on the
// build (BuildMode.hh):
//
//   patched  at StartChemistry: the delayed-list molecules are copied and the
//            chemistry is stopped before its first step. No chemistry random
//            number is drawn and nothing reacts.
//   stock    Geant4 gives no hook before the start-up contacts, so StartChemistry
//            only reseeds G4Random with ChemistrySeedFor(physics seed) and the
//            chemistry runs on. Geant4 first takes zero-time steps (the start-up
//            interaction steps, then the reactions of molecules that start in
//            contact). BeforeTimeStep copies the main-list molecules at the first
//            step of positive length, which is still at 1 ps, then stops the
//            chemistry. The packet is the state after the start-up contacts;
//            each molecule carries the id of the start-up reaction that made it
//            (PacketMolecule::productGroup, 0 for the others).
//
// capture.json records, per packet, the digest of the molecules waiting at
// chemistry start and the digest of the molecules captured (PacketDigest.hh).
//
// Errors never throw through the Geant4 kernel: the first one is stored
// (Failure()), the chemistry is stopped and the run is aborted. A state that the
// stock capture cannot interpret (a molecule still delayed, a stopped-but-alive
// track, a secondary not yet merged) is reported as PTIRT_CAPTURE_STATE.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
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
  // DigestMolecules of the molecules waiting in the delayed lists when the
  // chemistry started, and of the molecules in the packet. Equal in the patched
  // build; the stock build captures later, so they differ once a start-up
  // contact has changed or moved a molecule.
  std::uint64_t chemistryStartDigest = 0;
  std::uint64_t capturedDigest = 0;
  // Stock only (0 in the patched build): the seed given to G4Random when the
  // chemistry started, the zero-length steps taken before the capture and the
  // reactions that fired in them.
  std::uint64_t chemistrySeed = 0;
  int startupZeroSteps = 0;
  int startupReactions = 0;
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
  void StartChemistry();
  void BeforeTimeStep(G4double timeStep);
  void OnReaction(const G4Track& trackA, const G4Track& trackB,
                  const std::vector<G4Track*>* products);
  void EndEvent(const G4Event& event);

  const std::string& Failure() const { return fFailure; }
  const std::vector<LibraryEntry>& Entries() const { return fEntries; }
  const std::vector<CaptureProvenance>& Provenance() const { return fProvenance; }
  G4double GetCaptureHalfSide() const { return fHalfSide; }

 private:
  void Fail(const std::string& message);
  void CaptureStartupState();

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
  bool fCaptured = false;  // fMolecules holds the packet molecules
  std::vector<PacketMolecule> fMolecules;
  std::uint64_t fChemistryStartDigest = 0;
  std::uint64_t fCapturedDigest = 0;
  std::uint64_t fChemistrySeed = 0;
  int fStartupZeroSteps = 0;
  int fStartupReactions = 0;
  // Stock only: the molecules made by the start-up reactions so far -> the
  // 1-based number of the reaction. Keyed by track address, so the map does
  // not depend on when Geant4 assigns a product's track id (11.4.3 source
  // suggests it is already set in UserReactionAction; the first attempt keyed
  // by id failed, cause not established). A reaction removes its reactants from the map
  // (they die), so a later track that reuses an address is not mistaken for a
  // product, and a later product replaces a stale entry.
  std::map<const G4Track*, int> fProductReaction;
};

}  // namespace ptirt
