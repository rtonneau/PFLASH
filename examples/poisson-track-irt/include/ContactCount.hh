// ContactCount: how many pairs of molecules lie within their reaction radius.
// Geant4-free; the radius comes from a function the caller supplies.
//
// The stock build does not give a source its contact trials when it activates
// (the patched stepper does), so the history reports how many fresh-old pairs
// were in contact at that moment. This file is the counting core; ReactionRadius.hh
// builds the radius function from the Geant4 reaction table.
//
// Units: any length unit, the same for positions and radii (the example uses nm).
// Contact is strict: squared distance < radius squared. A radius < 0 means "these
// two species do not react"; a radius of 0 gives no contact either.
#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ptirt
{

struct ContactMolecule
{
  std::string species;
  std::array<double, 3> positionNm{};
};

// Effective reaction radius of two species, symmetric in its arguments; < 0 when
// they do not react.
using ContactRadiusFn = std::function<double(const std::string&, const std::string&)>;

struct ContactCounts
{
  std::size_t pairs = 0;                   // pairs in contact
  std::size_t newMoleculesInContact = 0;   // fresh molecules (internal: molecules) in at least one pair
  std::map<std::string, std::size_t> perPair;  // pairs by PairLabel
  double cpuSeconds = 0;                   // std::chrono::steady_clock around the counting loop
};

// "A + B" with the two names in sorted order, so that (B, A) and (A, B) agree.
std::string PairLabel(const std::string& a, const std::string& b);

// Pairs (fresh, old) in contact. newMoleculesInContact counts the fresh molecules
// that are in contact with at least one old molecule.
ContactCounts CountNewOldContacts(const std::vector<ContactMolecule>& fresh,
                                  const std::vector<ContactMolecule>& old, const ContactRadiusFn& radius);

// Pairs inside one set, each unordered pair once. newMoleculesInContact counts the
// molecules that are in at least one pair.
ContactCounts CountInternalContacts(const std::vector<ContactMolecule>& molecules,
                                    const ContactRadiusFn& radius);

}  // namespace ptirt
