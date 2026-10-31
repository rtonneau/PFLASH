/// Generates gun primaries and captures the first beam settings for each run.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per serial/worker thread; callbacks and mutable access stay on that thread.

#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"

class G4ParticleGun;

/// Generates gun primaries and captures the first beam settings for each run.
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
public:
  /// Owns a one-particle electron gun with 2 MeV defaults; requires initialized particle definitions.
  PrimaryGeneratorAction();
  /// Releases the owned gun after callbacks stop.
  virtual ~PrimaryGeneratorAction();

  /// Adds gun vertices to a non-null event, then captures beam settings once per worker run.
  /// Manifest units are keV and micrometres; undefined gun particle raises FatalException.
  virtual void GeneratePrimaries(G4Event *);

private:
  G4ParticleGun *fpParticleGun;
};
#endif
