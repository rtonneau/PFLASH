// Fixture 1, coincident molecules in the octree finder (patch
// coincident-neighbors.patch, docs/dose-rate/molecular-finder.md).
//
// e_aq + e_aq -> 2 OH- + H2 places its products at one point; the dose-rate
// study then rebuilds the finder with them. Stock G4Octree subdivides
// coincident entries to the depth limit with a shared ++current_depth, a finite
// but enormous tree that exhausts memory. Stock queries also drop every
// distance-zero entry, so coincident partners are never seen as neighbours.
//
// Run under JobLauncher (memory cap): on stock the cap is hit while building
// and the launcher reports the defect; on a patched kernel the tree builds and
// the position query returns the 3 coincident molecules and the near ones.
#include "PatchCheck.hh"

#include "G4Molecule.hh"
#include "G4OctreeFinder.hh"
#include "G4SystemOfUnits.hh"

#include <memory>
#include <vector>

using Tracks = std::vector<G4Track*>;
using Finder = G4OctreeFinder<G4Molecule, Tracks>;
using Results = std::vector<std::pair<Tracks::iterator, double>>;

int main(int argc, char** argv)
{
  const auto expect = PatchCheck::ParseExpect(argc, argv);
  PatchCheck::Checker c("finder-coincident", expect);
  c.Module("G4processes_core", "G4processes_core.dll");

  const G4ThreeVector origin(12.3 * nm, 23. * nm, 37. * nm);
  std::vector<G4ThreeVector> points(3, origin);  // the coincident products
  for (int i = 0; i < 6; ++i) {                   // ordinary neighbours, 0.5 to 3 nm
    G4ThreeVector d(0, 0, 0);
    d[i % 3] = (i < 3 ? 1. : -1.) * (0.5 + 0.5 * i) * nm;
    points.push_back(origin + d);
  }
  std::vector<std::unique_ptr<G4Track>> owned;
  Tracks tracks;
  for (const auto& p : points) {
    owned.push_back(std::make_unique<G4Track>());
    owned.back()->SetPosition(p);
    owned.back()->SetTrackID(static_cast<G4int>(owned.size()));
    tracks.push_back(owned.back().get());
  }

  auto* finder = Finder::Instance();
  std::cout << "BUILD_ENTER " << tracks.size() << " entries, 3 coincident" << std::endl;
  finder->BuildTreeMap({{7, &tracks}});
  std::cout << "BUILD_EXIT" << std::endl;

  // Position query at the coincident point, radius 1 nm: the 3 coincident
  // entries (distance 0) and the 0.5 nm neighbour are inside.
  Results inRange;
  finder->FindNearestInRange(origin, 1. * nm, inRange, true);  // exact sphere (the keyed overload widens R below its cutoff)
  int coincident = 0;
  for (const auto& e : inRange)
    if ((*e.first)->GetPosition() == origin) ++coincident;

  // Track query from one coincident molecule: the other two are its partners.
  Results partners;
  finder->FindNearest(*tracks[0], 7, 1. * nm, partners, true);
  int partnerCount = 0;
  bool self = false;
  for (const auto& e : partners) {
    if (*e.first == tracks[0]) self = true;
    else if ((*e.first)->GetPosition() == origin) ++partnerCount;
  }
  finder->Clear();

  c.Result("treeBuilt", 1);
  c.Result("coincidentInRange", coincident);
  c.Result("inRange", inRange.size());
  c.Result("coincidentPartners", partnerCount);
  c.Result("selfReturned", self ? 1 : 0);
  if (expect == PatchCheck::Expect::Fixed) {
    c.Check(coincident == 3, "the position query returns the 3 coincident molecules");
    c.Check(inRange.size() == 4, "and the 0.5 nm neighbour, nothing else");
    c.Check(partnerCount == 2 && !self, "the track query returns the 2 partners, not itself");
  }
  else {
    // Reached only if the stock tree fits in the cap: the query defect remains.
    c.Check(coincident == 0 || partnerCount == 0, "coincident partners are dropped by the query");
  }
  return c.Finish();
}
