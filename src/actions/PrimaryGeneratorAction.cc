/// \file PrimaryGeneratorAction.cc
/// \brief Implementation of the PrimaryGeneratorAction class

#include "actions/PrimaryGeneratorAction.hh"

#include "actions/Run.hh"

#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Event.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::PrimaryGeneratorAction()
    : G4VUserPrimaryGeneratorAction(), fpParticleGun(new G4ParticleGun(1))
{
  G4ParticleDefinition *particle = G4ParticleTable::GetParticleTable()->FindParticle("e-");

  // default gun parameters
  this->fpParticleGun->SetParticleDefinition(particle);
  this->fpParticleGun->SetParticleEnergy(2. * MeV);
  this->fpParticleGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
  this->fpParticleGun->SetParticlePosition(G4ThreeVector(0., 0., 0. * micrometer));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete this->fpParticleGun;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction::GeneratePrimaries(G4Event *anEvent)
{
  this->fpParticleGun->GeneratePrimaryVertex(anEvent);

  // Record the beam the gun actually has (set by /gun/* commands or the
  // constructor defaults) once per run, for the dump's manifest.
  auto *run = dynamic_cast<Run *>(G4RunManager::GetRunManager()->GetNonConstCurrentRun());
  if (run != nullptr && !run->HasBeam())
  {
    Run::Beam beam;
    beam.particle = this->fpParticleGun->GetParticleDefinition()->GetParticleName();
    beam.energy_keV = this->fpParticleGun->GetParticleEnergy() / keV;
    const G4ThreeVector position = this->fpParticleGun->GetParticlePosition();
    const G4ThreeVector direction = this->fpParticleGun->GetParticleMomentumDirection();
    beam.position_um[0] = position.x() / micrometer;
    beam.position_um[1] = position.y() / micrometer;
    beam.position_um[2] = position.z() / micrometer;
    beam.direction[0] = direction.x();
    beam.direction[1] = direction.y();
    beam.direction[2] = direction.z();
    run->SetBeamIfUnset(beam);
  }
}
