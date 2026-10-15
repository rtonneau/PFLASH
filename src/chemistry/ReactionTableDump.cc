/// \file ReactionTableDump.cc
/// \brief Implementation of the ReactionTableDump utility functions

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
  for (const auto* p : products) {
    productNames.push_back(p->GetName());
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
  std::ostringstream oss;
  oss << reactant1 << " + " << reactant2 << " ->";
  if (productNames.empty()) {
    oss << " (no products)";
  }
  else {
    for (std::size_t i = 0; i < productNames.size(); ++i) {
      oss << (i == 0 ? " " : " + ") << productNames[i];
    }
  }
  return oss.str();
}

void WriteBimolecular(std::ostream& out)
{
  auto* reactionTable = G4DNAMolecularReactionTable::GetReactionTable();
  auto reactions = reactionTable->GetVectorOfReactionData();

  for (const auto* rd : reactions) {
    std::vector<const G4MolecularConfiguration*> products;
    const G4int nbProducts = rd->GetNbProducts();
    for (G4int i = 0; i < nbProducts; ++i) {
      products.push_back(rd->GetProduct(i));
    }
    WriteLine(out, rd->GetReactant1()->GetName(), rd->GetReactant2()->GetName(),
             rd->GetObservedReactionRateConstant(), products);
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
    // the dump identical across runs and thread counts.
    std::vector<std::string> lines;
    for (const auto& [mol, materialMap] : access->GetReactionMap()) {
      for (const auto& [material, rd] : materialMap) {
        std::vector<const G4MolecularConfiguration*> products;
        const G4int nbProducts = rd->GetNbProducts();
        for (G4int i = 0; i < nbProducts; ++i) {
          products.push_back(rd->GetProduct(i));
        }
        std::ostringstream line;
        WriteLine(line, mol->GetName(), material->GetName(),
                  rd->GetObservedReactionRateConstant(), products);
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

  for (const auto* rd : reactionTable->GetVectorOfReactionData()) {
    std::vector<G4String> productNames;
    const G4int nbProducts = rd->GetNbProducts();
    productNames.reserve(nbProducts);
    for (G4int i = 0; i < nbProducts; ++i) {
      productNames.push_back(rd->GetProduct(i)->GetName());
    }
    gLabelCache.at(rd->GetReactionID() - 1) =
        FormatReactionLabel(rd->GetReactant1()->GetName(), rd->GetReactant2()->GetName(),
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
