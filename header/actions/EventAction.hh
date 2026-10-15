#ifndef EVENTACTION_HH
#define EVENTACTION_HH

#include "G4Event.hh"
#include "G4UserEventAction.hh"
#include "globals.hh"

class EventAction : public G4UserEventAction
{
public:
    EventAction();
    ~EventAction() override;

    void BeginOfEventAction(const G4Event *event) override;
    void EndOfEventAction(const G4Event *event) override;
};

#endif