// HistoryBoundedBrownianAction: keeps the products of the H2O dissociation
// inside the reflecting chemistry box (optional, halfBoxUm > 0).
//
// G4DNAMolecularDissociation places the products of an excited or ionised H2O
// with its displacer, then hands each product position to its user Brownian
// action (G4DNAMolecularDissociation::SetUserBrownianAction). This action folds
// a position that left the box back into it with ReflectIntoInterval, the
// example's own fold (ReflectingFold.hh), at the history boundaries (dissociation
// products and reaction products) in both the stock and the patched build.
// The in-box transport of every other molecule is still the kernel's
// G4ChemReboundTransportation in both builds; its fold is wrong for overshoots of
// two box lengths or more and at exactly one box length (on [0,1]: 2.1 -> 1.7,
// 2.9 -> -0.7, 1.0 -> 0.0). The Geant4-DNA UHDR example registers the same
// kernel class (EmDNAChemistry.cc), so it has the same limits. Same role as
// BoundedBrownianAction of the UHDR example. It holds its own copy of the box
// because the dissociation process deletes it only when the process table is
// destroyed, after main().
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
