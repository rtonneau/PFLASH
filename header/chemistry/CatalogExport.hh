/// \file CatalogExport.hh
/// Exports the live Chemistry catalogue (molecules, dissociation, reactions, bulk) as JSON.
/// Dependencies: Geant4, DnaLogger, DataNode, JsonWriter and ScavengerReactionAccess; not
/// standard-library-only.
/// Thread safety: Call from the master/only thread once the tables are final; no shared state.
///
/// The Make* functions are pure tree builders (no Geant4 state) and are unit tested;
/// BuildCatalog and WriteCatalog read the live Geant4 tables like ReactionTableDump.

#ifndef CatalogExport_h
#define CatalogExport_h 1

#include "scoring/DataNode.hh"

#include "globals.hh"

#include <string>
#include <vector>

namespace CatalogExport
{
/// Object {"reactants":[r1,r2],"products":[...],"k_M-1s-1":k,"type":type}.
DataNode MakeReactionNode(const std::string& reactant1, const std::string& reactant2,
                          const std::vector<std::string>& products, double k_M_inv_s_inv,
                          int type);

/// Object {"reactant","partner","products","k_M-1s-1"} (bulk reaction).
DataNode MakeBulkNode(const std::string& reactant, const std::string& partner,
                      const std::vector<std::string>& products, double k_M_inv_s_inv);

/// Object {"name","D_m2s-1","radius_nm","charge"}.
DataNode MakeMoleculeNode(const std::string& name, double diffusion_m2_s, double radius_nm,
                          int charge);

/// Reads the live molecule, decay and reaction tables. Requires final, frozen tables.
/// Keys: chemistry, timeStepModel, geant4Version, molecules, dissociation, reactions, bulk.
DataNode BuildCatalog(const std::string& chemistryName);

/// Writes BuildCatalog as JSON to filename. A failed open is a fatal G4Exception
/// "CannotOpenFile"; an Info line is logged on success.
void WriteCatalog(const G4String& filename, const std::string& chemistryName);
}  // namespace CatalogExport

#endif  // CatalogExport_h
