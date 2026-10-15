#include <G4SystemOfUnits.hh>
#include <G4Track.hh>
#include <G4UnitsTable.hh>
#include <actions/TrackingAction.hh>
#include <scoring/PrimaryKiller.hh>

TrackingAction::TrackingAction() : G4UserTrackingAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TrackingAction::PostUserTrackingAction(const G4Track *track)
{
  // Track 1 is the only primary (the gun fires one particle per event).
  if (track->GetTrackID() == 1)
  {
    fEventRow.primaryLength_nm = track->GetTrackLength() / nm;
    fEventRow.primaryEkin0_keV = track->GetVertexKineticEnergy() / keV;
    fEventRow.primaryEkinEnd_keV = track->GetKineticEnergy() / keV;
    // PrimaryKiller flags its own kills: a killed primary may still be inside
    // the world with a low kinetic energy, like a stopped one.
    if (PrimaryKiller::PrimaryKilledThisEvent())
      fEventRow.primaryEnd = TrackLengthTable::End::Killed;
    else if (track->GetNextVolume() == nullptr)
      fEventRow.primaryEnd = TrackLengthTable::End::Escaped;
    else
      fEventRow.primaryEnd = TrackLengthTable::End::Stopped;
    return;
  }

  // Secondary (and later-generation) electrons only.
  if (track->GetParticleDefinition()->GetPDGEncoding() != 11)
    return;

  const double length_nm = track->GetTrackLength() / nm;
  fEventRow.secondaryAll_nm += length_nm;
  ++fEventRow.nSecondaryAll;
  if (track->GetParentID() == 1)
  {
    fEventRow.secondaryFirstGen_nm += length_nm;
    ++fEventRow.nSecondaryFirstGen;
  }
}

void TrackingAction::PreUserTrackingAction(const G4Track * /*track*/)
{
  // if (track->GetParentID() == 0)
  // {
  //   G4cout << "New primary track, TrackID = "
  //          << track->GetTrackID()
  //          << ", Particle = " << track->GetDefinition()->GetParticleName()
  //          << G4endl;
  // }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TrackingAction::CommitEvent(int runId, int eventId)
{
  TrackLengthTable::Row row = fEventRow;
  row.run = runId;
  row.event = eventId;
  fTrackLengthTable.Add(row);
}

void TrackingAction::ResetEvent()
{
  fEventRow = TrackLengthTable::Row();
}
