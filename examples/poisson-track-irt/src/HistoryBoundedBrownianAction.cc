#include "HistoryBoundedBrownianAction.hh"

#include <algorithm>
#include <sstream>

#include "G4ChemReboundTransportation.hh"
#include "G4Exception.hh"
#include "G4Track.hh"

namespace ptirt
{

G4bool HistoryBoundedBrownianAction::Fold(G4ThreeVector& position, const G4DNABoundingBox& box)
{
  if (box.contains(position)) return false;
  // Reflection at each face, applied per axis (fatal on a non-finite coordinate).
  position.set(G4ChemReboundTransportation::calculateNextCoordinate(position.x(), box.Getxhi(), box.Getxlo()),
               G4ChemReboundTransportation::calculateNextCoordinate(position.y(), box.Getyhi(), box.Getylo()),
               G4ChemReboundTransportation::calculateNextCoordinate(position.z(), box.Getzhi(), box.Getzlo()));
  return true;
}

void HistoryBoundedBrownianAction::Transport(G4ThreeVector& position, G4Track*)
{
  if (Fold(position, fBox)) ++fFoldCount;
}

G4double HistoryBoundedBrownianAction::GetDistanceToBoundary(const G4Track& track)
{
  const auto& p = track.GetPosition();
  if (!fBox.contains(p))
  {
    std::ostringstream text;
    text << "track " << track.GetTrackID() << " at " << p << " is outside the reflecting box";
    G4Exception("HistoryBoundedBrownianAction::GetDistanceToBoundary", "PTIRT_HISTORY_BOX",
                FatalException, text.str().c_str());
  }
  return std::min({p.x() - fBox.Getxlo(), fBox.Getxhi() - p.x(), p.y() - fBox.Getylo(),
                   fBox.Getyhi() - p.y(), p.z() - fBox.Getzlo(), fBox.Getzhi() - p.z()});
}

}  // namespace ptirt
