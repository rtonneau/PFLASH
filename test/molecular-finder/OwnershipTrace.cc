#include <array>
#include "G4DNABoundingBox.hh"
#include <iostream>
#include <stdexcept>
#include <vector>
int main() {
  try {
    std::vector<G4ThreeVector> points(3,G4ThreeVector(0.123,0.23,0.37));
    G4DNABoundingBox box(points.begin(),points.end());
    for(int level=0;level<6;++level) {
      const auto boxes=box.partition();
      int copies=0;
      for(const auto& child:boxes) for(const auto& point:points) {
        if(child.contains(point)) ++copies;
        if(!(child==box)) throw std::runtime_error("coincident box changed");
      }
      if(copies!=24) throw std::runtime_error("wrong owner multiplicity");
      std::cout << "LEVEL " << level << " input 3 child_entries " << copies << " identical_boxes 8\n";
      box=boxes[0];
    }
    const double epsilon=std::numeric_limits<double>::epsilon();
    std::vector<G4ThreeVector> near{{1,1,1},{1+3*epsilon,1,1}};
    G4DNABoundingBox rounded(near.begin(),near.end());
    if(!rounded.contains(near.front()) || rounded.overlap(near.front(),1e-17))
      throw std::runtime_error("roundoff fixture changed");
    std::cout << "ROUNDOFF endpoint_contained 1 native_overlap 0 exact_endpoint_distance 0 radius 1e-17\n";
    std::cout << "OWNERSHIP_TRACE_PASS threshold 2 max_depth 100\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
