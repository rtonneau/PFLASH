// HistoryBoundedBrownianAction: keeps the products of the H2O dissociation
// inside the reflecting chemistry box (optional, halfBoxUm > 0).
//
// G4DNAMolecularDissociation places the products of an excited or ionised H2O
// with its displacer, then hands each product position to its user Brownian
// action (G4DNAMolecularDissociation::SetUserBrownianAction). This action folds
// a position that left the box back into it with the same reflection rule as
// G4ChemReboundTransportation (calculateNextCoordinate), the transport used for
// all other molecules when the box is on. Same role as BoundedBrownianAction of
// the Geant4-DNA UHDR example. It holds its own copy of the box because the
// dissociation process deletes it only when the process table is destroyed,
// after main().
#pragma once

#include <cstddef>

#include "G4DNABoundingBox.hh"
#include "G4ThreeVector.hh"
#include "G4VUserBrownianAction.hh"
#include "globals.hh"

namespace ptirt
{

class HistoryBoundedBrownianAction : public G4VUserBrownianAction
{
 public:
  explicit HistoryBoundedBrownianAction(const G4DNABoundingBox& box) : fBox(box) {}
  ~HistoryBoundedBrownianAction() override = default;

  // Folds `position` into the box (identity for a point inside it).
  void Transport(G4ThreeVector& position, G4Track* track = nullptr) override;

  // Distance to the nearest face; a track outside the box is a fatal error.
  G4double GetDistanceToBoundary(const G4Track& track) override;

  // Number of product positions that were outside the box and were folded.
  std::size_t GetFoldCount() const { return fFoldCount; }

  // The fold itself, also used for reaction products (ActivationObserver).
  // Returns true when `position` was outside the box and has been moved.
  static G4bool Fold(G4ThreeVector& position, const G4DNABoundingBox& box);

 private:
  G4DNABoundingBox fBox;
  std::size_t fFoldCount = 0;
};

}  // namespace ptirt
