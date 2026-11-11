// ConfigurationRadii: species labels for molecular configurations, and the
// effective reaction radius of two of them, for ContactCount.hh and
// CoincidenceCheck.hh. Needs Geant4 (header only; include it from the Geant4
// applications, not from ptirtCore).
//
// The radius is the effective reaction radius of the entry that
// G4DNAMolecularReactionTable holds for the two configurations, in nm; -1 when the
// table has no entry for them. The label is the configuration's name, the same
// text the species column of the history uses. Two configurations that share a
// name (excited water states do) get a "#<molecule id>" suffix on the later one,
// so that a label always names one configuration.
#pragma once

#include <map>
#include <string>
#include <utility>

#include "ContactCount.hh"

#include "G4DNAMolecularReactionTable.hh"
#include "G4MolecularConfiguration.hh"
#include "G4SystemOfUnits.hh"

namespace ptirt
{

class ConfigurationRadii
{
 public:
  // Registers the configuration (if new) and returns its label.
  const std::string& Label(const G4MolecularConfiguration* configuration)
  {
    if (const auto it = fLabelOf.find(configuration); it != fLabelOf.end()) return it->second;
    std::string label = configuration->GetName();
    if (fByLabel.count(label) != 0) label += "#" + std::to_string(configuration->GetMoleculeID());
    fByLabel[label] = configuration;
    return fLabelOf.emplace(configuration, std::move(label)).first->second;
  }

  // Effective reaction radius in nm; < 0 when the pair has no reaction data or a
  // label is unknown.
  double Radius(const std::string& labelA, const std::string& labelB) const
  {
    const auto a = fByLabel.find(labelA);
    const auto b = fByLabel.find(labelB);
    if (a == fByLabel.end() || b == fByLabel.end()) return -1;
    const auto* table = G4DNAMolecularReactionTable::GetReactionTable();
    const auto* data = table->GetReactionData(a->second, b->second);
    return data == nullptr ? -1.0 : data->GetEffectiveReactionRadius() / nm;
  }

  // Contact probability the IRT_syn stepper applies to a pair that starts within the
  // effective radius (G4DNAMolecularReactionData::GetProbability); 0 when the pair
  // has no reaction data or a label is unknown.
  double Probability(const std::string& labelA, const std::string& labelB) const
  {
    const auto a = fByLabel.find(labelA);
    const auto b = fByLabel.find(labelB);
    if (a == fByLabel.end() || b == fByLabel.end()) return 0;
    const auto* data = G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(a->second, b->second);
    return data == nullptr ? 0.0 : data->GetProbability();
  }

  // Valid while this object lives.
  ContactRadiusFn AsFunction() const
  {
    return [this](const std::string& a, const std::string& b) { return Radius(a, b); };
  }

 private:
  std::map<std::string, const G4MolecularConfiguration*> fByLabel;
  std::map<const G4MolecularConfiguration*, std::string> fLabelOf;
};

}  // namespace ptirt
