/// \file ChemistryRegistry.hh
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
  /// name already registered (compared case-insensitively).
  bool Register(const Chemistry& chemistry, std::string& err);

  /// Picks the Chemistry, matching name case-insensitively. False + err if the
  /// name is unknown (err lists the valid names) or if a different Chemistry was
  /// already picked. Picking the same one again returns true and changes nothing.
  bool Select(const std::string& name, std::string& err);

  /// The picked Chemistry, else the entry named kDefaultName, else nullptr.
  const Chemistry* Selected();

  /// Registered names, in registration order.
  std::vector<std::string> Names();

  /// Clears every registration and the selection. Tests only.
  void ResetForTesting();
}  // namespace ChemistryRegistry

#endif  // ChemistryRegistry_h
