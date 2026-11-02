/// \file ScavengerReactionAccess.hh
/// Exposes a scavenger process reaction map for inspection.
/// Dependencies: Geant4-dependent; not standard-library-only.
/// Thread safety: Inspect the owning thread process only after its reaction map is built.
///
/// Thin subclass exposing G4DNAScavengerProcess's protected per-molecule
/// reaction map for read access. Adds no data and no behavior -- it exists
/// solely so a reaction-table dump can read each process's own live state
/// directly, instead of a project-side copy recorded at construction time.

#ifndef ScavengerReactionAccess_h
#define ScavengerReactionAccess_h 1

#include "G4DNAScavengerProcess.hh"

/// Geant4 scavenger process with borrowed read access to its registered reactions.
class ScavengerReactionAccess : public G4DNAScavengerProcess
{
public:
  /// Uses Geant4 constructors and their boundary/initialization requirements.
  using G4DNAScavengerProcess::G4DNAScavengerProcess;

  /// Borrowed map for the process lifetime; do not mutate through Data pointers.
  /// Read only after registration, with no concurrent mutation; no failure path.
  const std::map<MolType, std::map<MolType, Data*>>& GetReactionMap() const { return fConfMap; }
};

#endif // ScavengerReactionAccess_h
