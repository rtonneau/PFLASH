#include "BoundedBrownianAction.hh"
#include "G4DNABoundingBox.hh"
#include "G4ChemReboundTransportation.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include <cmath>
#include <stdexcept>
#include <iostream>
int main() {
 try {
  G4DNABoundingBox box({2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um,2*CLHEP::um,-2*CLHEP::um});
  BoundedBrownianAction action; action.SetBoundary(box);
  for(int axis=0;axis<3;++axis) for(int side:{-1,1}) for(double distance:{0.1,2.0,5.0}) {
   G4ThreeVector p; p[axis]=side*(2.0+distance)*CLHEP::um;
   const double rebound=G4ChemReboundTransportation::calculateNextCoordinate(p[axis],2*CLHEP::um,-2*CLHEP::um);
   action.BouncingAction(p);
   if(std::abs(p[axis]-rebound)>1e-12*CLHEP::um) {std::cerr<<"axis "<<axis<<" side "<<side<<" distance_um "<<distance<<" transport_um "<<rebound/CLHEP::um<<" dissociation_um "<<p[axis]/CLHEP::um<<"\n"; throw std::runtime_error("transport and dissociation reflection disagree");}
   if(!box.contains(p)) throw std::runtime_error("reflected point outside box");
   const double expected=side*(distance==5.0 ? -1.0 : 2.0-distance)*CLHEP::um;
   if(std::abs(p[axis]-expected)>1e-12*CLHEP::um) throw std::runtime_error("folded coordinate incorrect");
  }
  std::cout<<"BOUNDARY_REFLECTION_PASS\n"; return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
