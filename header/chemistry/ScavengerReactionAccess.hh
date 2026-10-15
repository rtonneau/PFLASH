/// \file ScavengerReactionAccess.hh
/// \brief Definition of the ScavengerReactionAccess class
///
/// Thin subclass exposing G4DNAScavengerProcess's protected per-molecule
/// reaction map for read access. Adds no data and no behavior -- it exists
/// solely so a reaction-table dump can read each process's own live state
/// directly, instead of a project-side copy recorded at construction time.

#ifndef ScavengerReactionAccess_h
#define ScavengerReactionAccess_h 1

#include "G4DNAScavengerProcess.hh"

class ScavengerReactionAccess : public G4DNAScavengerProcess
{
public:
  using G4DNAScavengerProcess::G4DNAScavengerProcess;

  const std::map<MolType, std::map<MolType, Data*>>& GetReactionMap() const { return fConfMap; }
};

#endif // ScavengerReactionAccess_h
