/// \file ScavengerSpec.hh
/// \brief /chem/env/scavenger values: parsing, conversion to molarity, and the
/// checks that need no Geant4 kernel.
///
/// Pure logic (standard library + ChemistryTypes.hh): no Geant4 kernel, no
/// logging. Errors are returned as bool + message; the caller raises the
/// G4Exception. Molarities are plain mol/L doubles; DnaChemistryWorld applies
/// Geant4 units. Unit rules follow the UHDR example's ChemistryWorld.
#ifndef ScavengerSpec_h
#define ScavengerSpec_h 1

#include "chemistry/ChemistryTypes.hh"

#include <string>
#include <vector>

namespace ScavengerSpec
{
  /// O2 molarity in water under a pure-O2 atmosphere (Henry's law, UHDR), in M.
  constexpr double kO2SaturationMolarity = 0.0013;

  /// One exogenous bulk scavenger: species as named in G4MoleculeTable,
  /// concentration in mol/L (0 = absent).
  struct Entry
  {
    std::string species;
    double molarity = 0.;
  };

  using List = std::vector<Entry>;

  /// Parses "<species> <value> <unit>". unit: M, mM, uM, or % (O2 only, % of
  /// kO2SaturationMolarity). False + err for a wrong token count, a
  /// non-numeric or non-finite value, a negative value, an unknown unit, % on
  /// a species other than O2, or a pH-owned species (IsPhOwned).
  bool Parse(const std::string& text, Entry& out, std::string& err);

  /// True for the bulk species set by the water/pH model: H2O, H2O(B),
  /// H3Op(B), OHm(B).
  bool IsPhOwned(const std::string& species);

  /// Replaces the entry with the same species (last wins) or appends it.
  void Upsert(List& list, const Entry& entry);

  /// Species with molarity > 0 that are not the partner of any reaction in
  /// `reactions`, in list order: they would sit in the bulk without reacting.
  std::vector<std::string> InertSpecies(const List& list,
                                        const ChemistryTypes::BulkReactionList& reactions);
}  // namespace ScavengerSpec

#endif  // ScavengerSpec_h
