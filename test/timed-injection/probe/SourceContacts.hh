#pragma once
#include "G4Track.hh"
#include "G4IT.hh"
#include "G4Scheduler.hh"
#include <map>
#ifdef PROBE_PRIVATE_ACTIVATION
#include "G4DNAIndependentReactionTimeStepper.hh"
#endif
namespace SourceContacts {
inline thread_local bool enabled=false;
inline thread_local double unusedMinTimeStep=0;
inline thread_local std::map<int,double> activation;
inline thread_local long long newNew=0,newOld=0,oldOld=0;
inline void ResetPrivateHistory() {
#ifdef PROBE_PRIVATE_ACTIVATION
 G4DNAIndependentReactionTimeStepper::ResetSourceActivations();
#endif
}
inline void RegisterPrivateSource(int id,double time) {
#ifdef PROBE_PRIVATE_ACTIVATION
 if(enabled) G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(id,time);
#endif
}
inline bool Fresh(const G4Track& t) {
 const double now=G4Scheduler::Instance()->GetGlobalTime();
 const auto it=activation.find(t.GetTrackID());
 if(it!=activation.end()) return it->second==now;
 // Unimolecular products inherit G4Track parent ID (G4ITStepProcessor2.cc).
 // Bimolecular products inherit the two G4IT parents (G4ITModelProcessor.cc).
 int a=0,b=0; GetIT(const_cast<G4Track*>(&t))->GetParentID(a,b);
 for(int id:{t.GetParentID(),a,b}) {
  const auto parent=activation.find(id);
  if(id!=0 && parent!=activation.end() && parent->second==now) {
   activation[t.GetTrackID()]=now;return true;
  }
 }
 return false;
}
inline bool AtContact(const G4Track& a,const G4Track& b) {
 if(G4Scheduler::Instance()->GetGlobalTime()==G4Scheduler::Instance()->GetStartTime()) return true;
 bool x=Fresh(a),y=Fresh(b);
 if(x&&y) ++newNew; else if(x||y) ++newOld; else ++oldOld;
 return enabled&&(x||y);
}
}
