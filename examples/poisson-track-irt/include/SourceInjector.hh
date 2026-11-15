// SourceInjector: puts the precursor packets of an arrival schedule into ONE
// shared IRT_syn chemistry history.
//
// Approach. Every primary was transported on its own by `capture`, and the
// molecules of its track were captured at the precursor age (1 ps, ticket 02).
// Here each packet of the schedule (one "source") is rigidly translated by its
// offset and pushed, before the chemistry starts, into the G4ITTrackHolder of a
// single standalone chemistry run (G4DNAChemistryManager::UseAsStandalone(true)
// then Run(), see HistoryMain.cc). A source that arrives at time t is pushed
// with global time t + age. G4ITTrackHolder keeps tracks pushed before the run
// in delayed lists keyed by their global time, and G4Scheduler::Process merges
// each list into the running history only when the history clock reaches that
// time: each source activates at its own time, among the molecules left by the
// earlier sources, and all sources react with each other.
//
// For every molecule the injector
//   - gives it a positive track ID, unique over the history (1, 2, ... in
//     schedule order); molecules made during the history get negative IDs from
//     G4ITTrackHolder, so they never collide;
//   - registers it with the patched IRT_syn stepper,
//     G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(id, t + age),
//     so the stepper treats it as a fresh source at its activation time (the
//     registry is cleared first with ResetSourceActivations, once per history);
//   - restores the captured state: configuration, track status, direction,
//     kinetic energy, local and proper times. The position is translated only:
//     the primary direction is not applied (packets are not rotated), by design.
// Pre-dissociation H2O states are injected as captured, so option3's H2O
// dissociation runs inside the shared history.
//
// Molecule counter: G4Molecule::BuildTrack counts a molecule at once when the
// G4MoleculeCounterManager is active, which would count every source at the
// start. The tracks are therefore built with the manager inactive, and
// ActivationObserver counts each source when it actually activates.
//
// Everything is validated in the constructor, before any Geant4 state changes:
// a packet molecule must resolve to exactly one molecular configuration of this
// build (else "incompatible packet"), sit at the packet's capture age, land
// strictly inside the world (and the reflecting box, if any) after translation,
// and its source must activate before the endpoint.
#pragma once

#include <cstddef>
#include <vector>

#include "ArrivalSchedule.hh"
#include "CaptureLibrary.hh"
#include "Ledger.hh"

#include "G4ThreeVector.hh"
#include "G4TrackStatus.hh"
#include "globals.hh"

class G4MolecularConfiguration;

namespace ptirt
{

// One source as injected; ActivationObserver checks the history against it.
struct InjectedSource
{
  std::size_t sourceIndex = 0;  // Arrival::sourceIndex (kept in single mode)
  G4double activationTime = 0;  // Geant4 units: arrival time + packet capture age
  G4int firstTrackId = 0;       // track IDs firstTrackId..lastTrackId, contiguous
  G4int lastTrackId = 0;
};

class SourceInjector
{
 public:
  // Needs the molecule table (after /run/initialize). worldHalfSide and
  // boxHalfSide (0 = no reflecting box) and endpoint are in Geant4 units.
  // Throws std::runtime_error ("source S (packet P): ...") on any violation.
  SourceInjector(const std::vector<Arrival>& arrivals, const PacketLibrary& library,
                 G4double worldHalfSide, G4double boxHalfSide, G4double endpoint);

  // Pushes every molecule into the delayed lists (once, before the chemistry
  // runs). Throws std::runtime_error if called twice, while the scheduler runs,
  // or if the molecule counter manager is not active.
  void Inject();

  const std::vector<InjectedSource>& Sources() const { return fSources; }
  std::size_t MoleculeCount() const { return fMolecules.size(); }

  // One record per source, observedActivationTimeNs = NaN (filled by the caller).
  std::vector<ActivationRecord> MakeActivationRecords() const;

 private:
  struct Molecule
  {
    const G4MolecularConfiguration* configuration = nullptr;
    G4ThreeVector position;
    G4ThreeVector direction;
    G4TrackStatus status = fAlive;
    G4double kineticEnergy = 0;
    G4double localTime = 0;
    G4double properTime = 0;
    std::size_t source = 0;  // position in fSources
  };

  std::vector<InjectedSource> fSources;
  std::vector<ActivationRecord> fRecords;
  std::vector<Molecule> fMolecules;  // in track-ID order: molecule i gets ID i + 1
  G4bool fInjected = false;
};

}  // namespace ptirt
