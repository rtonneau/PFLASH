/// \file BuiltInChemistries.hh
/// Registers the compiled Chemistry builders.
/// Dependencies: Geant4 and project registry; not standard-library-only.
/// Thread safety: Call on the master before workers; the registration guard is not synchronized.
/// \brief Registers the Chemistries compiled into this project.
#ifndef BuiltInChemistries_h
#define BuiltInChemistries_h 1

namespace BuiltInChemistries
{
  /// Registers every built-in Chemistry with ChemistryRegistry. Safe to call
  /// more than once; only the first call registers. Raises a fatal G4Exception
  /// if a registration is rejected (a programming error, e.g. a duplicate name).
  /// Master-only before registry readers start. ResetForTesting does not reset its guard.
  void Register();
}  // namespace BuiltInChemistries

#endif  // BuiltInChemistries_h
