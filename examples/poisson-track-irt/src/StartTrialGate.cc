#include "StartTrialGate.hh"

#include "G4DNAMolecularReactionTable.hh"

namespace ptirt
{

StartTrialGate::StartTrialGate(G4DNAMolecularReactionTable& table) : StartTrialGate(table.GetVectorOfReactionData())
{
}

StartTrialGate::StartTrialGate(const std::vector<const G4DNAMolecularReactionData*>& data)
{
  // The only allocation comes first: once a probability is changed, nothing can
  // throw before it is recorded for the destructor.
  fSaved.reserve(data.size());
  for (const auto* entry : data)
  {
    if (entry == nullptr) continue;
    auto* mutableEntry = const_cast<G4DNAMolecularReactionData*>(entry);
    fSaved.emplace_back(mutableEntry, mutableEntry->GetProbability());
    mutableEntry->SetProbability(0.);
  }
}

StartTrialGate::~StartTrialGate()
{
  // Reverse order: a data listed twice gets the value it had before the gate.
  for (auto saved = fSaved.rbegin(); saved != fSaved.rend(); ++saved) saved->first->SetProbability(saved->second);
}

}  // namespace ptirt
