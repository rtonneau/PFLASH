/// \file StackingAction.hh
/// \brief Definition of the StackingAction class

#ifndef G4ITStackingAction_h
#define G4ITStackingAction_h 1

#include "G4UserStackingAction.hh"
#include "globals.hh"

#include "physics/PhysicsList.hh"

class StackingAction : public G4UserStackingAction
{
public:
  StackingAction();
  virtual ~StackingAction() { ; }
  virtual void NewStage();

private:
  const PhysicsList *physList;
};

#endif
