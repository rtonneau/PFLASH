/// \file ChemistryRegistry.hh
/// Stores Chemistry builders and the process-wide selection.
/// Dependencies: Standard library plus the portable ChemistryTypes data header.
/// Thread safety: Mutate only before workers start; subsequent registry access is read-only.
/// \brief Name -> Chemistry registry and the once-per-process selection.
///
/// Pure logic: no Geant4 kernel, no logging, no process exit. Portable (needs
/// ChemistryTypes.hh only). Not thread-synchronized: Register/Select run from
/// the master thread before /run/initialize; afterwards everything is read-only.
#ifndef ChemistryRegistry_h
#define ChemistryRegistry_h 1

#include "chemistry/ChemistryTypes.hh"

#include <string>
#include <vector>

class G4DNAMolecularReactionTable;

namespace ChemistryRegistry
{
  /// Named builder callbacks; required builders must be non-null for Register.
  /// The registry stores callbacks but does not invoke or own their target state.
  struct Chemistry
  {
    std::string name;
    void (*buildReactions)(G4DNAMolecularReactionTable*);
    ChemistryTypes::BulkReactionList (*buildBulkReactions)();
    /// Optional: creates molecules this Chemistry needs beyond the shared set.
    /// Null = none. Runs on the master thread from DnaChemistryList::ConstructMolecule
    /// and from /chem/select (ChemistrySelectMessenger), so an implementation must be
    /// idempotent (never `new G4MoleculeDefinition`
    /// for a name that may already exist). See ADR 0007.
    void (*constructMolecules)() = nullptr;
  };

  /// Chemistry used when no /chem/select was issued.
  extern const char* const kDefaultName;

  /// Adds a Chemistry. False + err for an empty name, a null builder, or a
  /// name already registered (compared case-insensitively). Master-only before
  /// readers start. Success leaves err unchanged; allocation exceptions propagate.
  bool Register(const Chemistry& chemistry, std::string& err);

  /// Picks the Chemistry, matching name case-insensitively. False + err if the
  /// name is unknown (err lists the valid names) or if a different Chemistry was
  /// already picked. Selecting the same name again preserves the selection.
  /// Master-only before readers start; success leaves err unchanged.
  bool Select(const std::string& name, std::string& err);

  /// The picked Chemistry, else kDefaultName, else nullptr. Borrowed pointer:
  /// Register may invalidate it; ResetForTesting always invalidates it.
  const Chemistry* Selected();

  /// Copies registered names in registration order; allocation exceptions propagate.
  std::vector<std::string> Names();

  /// Clears registrations and selection. Tests only, with no concurrent readers.
  /// Does not reset BuiltInChemistries::Register's separate once-only guard.
  void ResetForTesting();
}  // namespace ChemistryRegistry

#endif  // ChemistryRegistry_h
