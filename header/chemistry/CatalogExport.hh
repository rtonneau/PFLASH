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

#include <set>
#include <string>
#include <vector>

class G4DNAMolecularReactionData;
class G4MolecularConfiguration;

namespace CatalogExport
{
/// Key of the rate constant for a reaction order: "k_s-1" for order 1 (H2O partner,
/// which Geant4 uses as a first-order rate), "k_M-1s-1" for order 2 (every other pair).
std::string RateKey(int order);

/// Object {"reactants":[r1,r2],"products":[...],"order":order,<RateKey(order)>:k,
/// "type":type,"mesoOnly":bool}. k is in s^-1 (order 1) or M^-1 s^-1 (order 2).
/// mesoOnly marks entries added to the shared table only for the mesoscopic stage.
DataNode MakeReactionNode(const std::string& reactant1, const std::string& reactant2,
                          const std::vector<std::string>& products, double k, int order,
                          int type, bool mesoOnly);

/// Object {"reactant","partner","products","order",<RateKey(order)>} (bulk reaction).
DataNode MakeBulkNode(const std::string& reactant, const std::string& partner,
                      const std::vector<std::string>& products, double k, int order);

/// Object {"name","D_m2s-1","radius_nm","charge","configuration"}. name is the species
/// id used in all other sections (SpeciesId); configuration is the Geant4 configuration name.
DataNode MakeMoleculeNode(const std::string& name, double diffusion_m2_s, double radius_nm,
                          int charge, const std::string& configuration);

/// Species id used throughout the catalogue: the configuration's user ID (e.g. "e_aq",
/// "H3Op", "°OH") when set, else its Geant4 name.
std::string SpeciesId(const G4MolecularConfiguration* configuration);

/// Reads the live molecule, decay and reaction tables. Requires final, frozen tables.
/// mesoOnlyReactions are the reaction-table entries the driver added for the mesoscopic
/// stage only (DnaChemistryList::MesoOnlyReactions). Read-only, master thread.
/// Keys: chemistry, timeStepModel, geant4Version, molecules, dissociation, reactions, bulk.
DataNode BuildCatalog(const std::string& chemistryName,
                      const std::set<const G4DNAMolecularReactionData*>& mesoOnlyReactions);

/// Writes BuildCatalog as JSON to filename. A failed open is a fatal G4Exception
/// "CannotOpenFile"; an Info line is logged on success.
void WriteCatalog(const G4String& filename, const std::string& chemistryName,
                  const std::set<const G4DNAMolecularReactionData*>& mesoOnlyReactions);
}  // namespace CatalogExport

#endif  // CatalogExport_h
