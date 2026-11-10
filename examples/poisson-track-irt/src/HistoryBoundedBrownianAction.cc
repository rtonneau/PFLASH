#include "HistoryBoundedBrownianAction.hh"

#include <algorithm>
#include <sstream>

#include "ReflectingFold.hh"

#include "G4Exception.hh"
#include "G4Track.hh"

namespace ptirt
{

G4bool HistoryBoundedBrownianAction::Fold(G4ThreeVector& position, const G4DNABoundingBox& box)
{
  if (box.contains(position)) return false;
  // Reflection at each face, applied per axis with the example's own fold
  // (throws std::invalid_argument on a non-finite coordinate).
  position.set(ReflectIntoInterval(position.x(), box.Getxlo(), box.Getxhi()),
               ReflectIntoInterval(position.y(), box.Getylo(), box.Getyhi()),
               ReflectIntoInterval(position.z(), box.Getzlo(), box.Getzhi()));
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
