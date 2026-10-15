/// \file BuiltInChemistries.hh
/// \brief Registers the Chemistries compiled into this project.
#ifndef BuiltInChemistries_h
#define BuiltInChemistries_h 1

namespace BuiltInChemistries
{
  /// Registers every built-in Chemistry with ChemistryRegistry. Safe to call
  /// more than once; only the first call registers. Raises a fatal G4Exception
  /// if a registration is rejected (a programming error, e.g. a duplicate name).
  void Register();
}  // namespace BuiltInChemistries

#endif  // BuiltInChemistries_h
