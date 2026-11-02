/// \file ReactionTableDump.cc
/// Formats and exports the live Chemistry reaction networks.
/// Dependencies: Geant4, DnaLogger and ScavengerReactionAccess; not standard-library-only.
/// Thread safety: Freeze tables before reads; callers own streams; LabelFor initializes its cache once.

#include "chemistry/ReactionTableDump.hh"

#include "core/DnaLogger.hh"
#include "chemistry/ScavengerReactionAccess.hh"

#include "G4DNAMolecularReactionTable.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4ProcessTable.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const G4double kRateUnit = 1e-3 * m3 / (mole * s);  // M^-1 s^-1

void WriteLine(std::ostream& out, const G4String& reactant1, const G4String& reactant2,
               G4double rate,
               const std::vector<const G4MolecularConfiguration*>& products)
{
  std::vector<G4String> productNames;
  productNames.reserve(products.size());
  for (const auto* product : products) {
    productNames.push_back(product->GetName());
  }
  out << ReactionTableDump::FormatReactionLabel(reactant1, reactant2, productNames) << "    k = "
      << (rate / kRateUnit) << " M^-1 s^-1\n";
}
}  // namespace

namespace ReactionTableDump
{
G4String FormatReactionLabel(const G4String& reactant1, const G4String& reactant2,
                              const std::vector<G4String>& productNames)
{
  std::ostringstream label;
  label << reactant1 << " + " << reactant2 << " ->";
  if (productNames.empty()) {
    label << " (no products)";
  }
  else {
    for (std::size_t i = 0; i < productNames.size(); ++i) {
      label << (i == 0 ? " " : " + ") << productNames[i];
    }
  }
  return label.str();
}

void WriteBimolecular(std::ostream& out)
{
  auto* reactionTable = G4DNAMolecularReactionTable::GetReactionTable();
  auto reactions = reactionTable->GetVectorOfReactionData();

  for (const auto* reactionData : reactions) {
    std::vector<const G4MolecularConfiguration*> products;
    const G4int nbProducts = reactionData->GetNbProducts();
    for (G4int i = 0; i < nbProducts; ++i) {
      products.push_back(reactionData->GetProduct(i));
    }
    WriteLine(out, reactionData->GetReactant1()->GetName(), reactionData->GetReactant2()->GetName(),
             reactionData->GetObservedReactionRateConstant(), products);
  }
}

void WriteBulkReactions(std::ostream& out)
{
  auto* processTable = G4ProcessTable::GetProcessTable();
  auto* moleculeTable = G4MoleculeTable::Instance();

  auto iterator = moleculeTable->GetDefintionIterator();
  iterator.reset();
  while (iterator()) {
    auto* moleculeDef = iterator.value();

    auto* process = processTable->FindProcess("G4DNAScavengerProcess", moleculeDef);
    auto* access = dynamic_cast<ScavengerReactionAccess*>(process);
    if (access == nullptr) {
      continue;
    }

    // The reaction map's iteration order is not stable from one process to
    // the next, so this molecule's lines are sorted before writing to keep
    // this molecule's bulk lines stable; other sections retain table order.
    std::vector<std::string> lines;
    for (const auto& [mol, materialMap] : access->GetReactionMap()) {
      for (const auto& [material, reactionData] : materialMap) {
        std::vector<const G4MolecularConfiguration*> products;
        const G4int nbProducts = reactionData->GetNbProducts();
        for (G4int i = 0; i < nbProducts; ++i) {
          products.push_back(reactionData->GetProduct(i));
        }
        std::ostringstream line;
        WriteLine(line, mol->GetName(), material->GetName(),
                  reactionData->GetObservedReactionRateConstant(), products);
        lines.push_back(line.str());
      }
    }
    std::sort(lines.begin(), lines.end());
    for (const auto& line : lines) {
      out << line;
    }
  }
}

void DumpReactionTable(const G4String& filename)
{
  std::ofstream out(filename);
  if (!out.is_open()) {
    G4Exception("ReactionTableDump::DumpReactionTable", "CannotOpenFile", FatalException,
               (G4String("Could not open reaction dump file: ") + filename).c_str());
    return;
  }

  out << "# Bimolecular reactions (pure water + O2 network)\n";
  WriteBimolecular(out);
  out << "\n# Bulk reactions (acid-base buffer + scavengers)\n";
  WriteBulkReactions(out);

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[ReactionTableDump] reaction table written to " + filename);
}

namespace
{
std::once_flag gLabelCacheOnce;
std::vector<G4String> gLabelCache;

void BuildLabelCache()
{
  auto* reactionTable = G4DNAMolecularReactionTable::GetReactionTable();
  gLabelCache.assign(reactionTable->GetNReactions(), G4String());

  for (const auto* reactionData : reactionTable->GetVectorOfReactionData()) {
    std::vector<G4String> productNames;
    const G4int nbProducts = reactionData->GetNbProducts();
    productNames.reserve(nbProducts);
    for (G4int i = 0; i < nbProducts; ++i) {
      productNames.push_back(reactionData->GetProduct(i)->GetName());
    }
    gLabelCache.at(reactionData->GetReactionID() - 1) =
        FormatReactionLabel(reactionData->GetReactant1()->GetName(), reactionData->GetReactant2()->GetName(),
                            productNames);
  }
}
}  // namespace

const G4String& LabelFor(G4int reactionID)
{
  std::call_once(gLabelCacheOnce, BuildLabelCache);
  return gLabelCache.at(reactionID - 1);
}
}  // namespace ReactionTableDump
