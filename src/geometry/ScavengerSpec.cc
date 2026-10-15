/// \file ScavengerSpec.cc
/// \brief Implementation of the ScavengerSpec parsing and checks
#include "geometry/ScavengerSpec.hh"

#include <cmath>
#include <exception>
#include <set>
#include <sstream>

bool ScavengerSpec::Parse(const std::string& text, Entry& out, std::string& err)
{
  std::istringstream iss(text);
  std::string species;
  std::string valueText;
  std::string unit;
  std::string extra;
  if (!(iss >> species >> valueText >> unit) || (iss >> extra)) {
    err = "expected '<species> <value> <unit>', got '" + text + "'";
    return false;
  }

  if (IsPhOwned(species)) {
    err = species + " is set by the water/pH model (/chem/env/pH), not by /chem/env/scavenger";
    return false;
  }

  double value = 0.;
  std::size_t used = 0;
  try {
    value = std::stod(valueText, &used);
  }
  catch (const std::exception&) {
    used = 0;
  }
  if (used == 0 || used != valueText.size() || !std::isfinite(value)) {
    err = "invalid concentration '" + valueText + "' for " + species;
    return false;
  }
  if (value < 0.) {
    err = "negative concentration " + valueText + " for " + species;
    return false;
  }

  double factor = 0.;
  if (unit == "M") {
    factor = 1.;
  }
  else if (unit == "mM") {
    factor = 1e-3;
  }
  else if (unit == "uM") {
    factor = 1e-6;
  }
  else if (unit == "%") {
    if (species != "O2") {
      err = "unit % is only defined for O2 (Henry's law, kH = 0.0013 M), not for " + species;
      return false;
    }
    factor = kO2SaturationMolarity / 100.;
  }
  else {
    err = "unknown unit '" + unit + "' for " + species + " (use M, mM, uM, or % for O2)";
    return false;
  }

  out = {species, value * factor};
  return true;
}

bool ScavengerSpec::IsPhOwned(const std::string& species)
{
  return species == "H2O" || species == "H2O(B)" || species == "H3Op(B)" || species == "OHm(B)";
}

void ScavengerSpec::Upsert(List& list, const Entry& entry)
{
  for (auto& existing : list) {
    if (existing.species == entry.species) {
      existing.molarity = entry.molarity;
      return;
    }
  }
  list.push_back(entry);
}

std::vector<std::string> ScavengerSpec::InertSpecies(
  const List& list, const ChemistryTypes::BulkReactionList& reactions)
{
  std::set<std::string> partners;
  for (const auto& entry : reactions) {
    for (const auto& reaction : entry.reactions) {
      partners.insert(reaction.partner);
    }
  }

  std::vector<std::string> inert;
  for (const auto& scavenger : list) {
    if (scavenger.molarity > 0. && partners.count(scavenger.species) == 0) {
      inert.push_back(scavenger.species);
    }
  }
  return inert;
}
