/// \file CatalogExport.cc
/// Exports the live Chemistry catalogue (molecules, dissociation, reactions, bulk) as JSON.
/// Dependencies: Geant4, DnaLogger, DataNode, JsonWriter and ScavengerReactionAccess; not
/// standard-library-only.
/// Thread safety: Call from the master/only thread once the tables are final; no shared state.

#include "chemistry/CatalogExport.hh"

#include "chemistry/ScavengerReactionAccess.hh"
#include "core/DnaLogger.hh"
#include "scoring/JsonWriter.hh"

#include "G4ChemTimeStepModel.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4EmParameters.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MolecularDissociationChannel.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4ProcessTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4Version.hh"

#include <algorithm>
#include <fstream>
#include <tuple>
#include <utility>

namespace
{
const G4double kRateUnit = 1e-3 * m3 / (mole * s);  // M^-1 s^-1

DataNode StringArray(const std::vector<std::string>& values)
{
  DataNode array = DataNode::MakeArray();
  for (const auto& value : values) {
    array.Push(value);
  }
  return array;
}

std::vector<std::string> ProductNames(const G4DNAMolecularReactionData* data)
{
  std::vector<std::string> names;
  for (G4int i = 0; i < data->GetNbProducts(); ++i) {
    names.push_back(data->GetProduct(i)->GetName());
  }
  return names;
}

std::string VersionString()
{
  return std::to_string(G4VERSION_NUMBER / 100) + "." +
         std::to_string(G4VERSION_NUMBER / 10 % 10) + "." + std::to_string(G4VERSION_NUMBER % 10);
}
}  // namespace

namespace CatalogExport
{
DataNode MakeReactionNode(const std::string& reactant1, const std::string& reactant2,
                          const std::vector<std::string>& products, double k_M_inv_s_inv,
                          int type)
{
  DataNode node = DataNode::MakeObject();
  node.Add("reactants", StringArray({reactant1, reactant2}));
  node.Add("products", StringArray(products));
  node.Add("k_M-1s-1", k_M_inv_s_inv);
  node.Add("type", type);
  return node;
}

DataNode MakeBulkNode(const std::string& reactant, const std::string& partner,
                      const std::vector<std::string>& products, double k_M_inv_s_inv)
{
  DataNode node = DataNode::MakeObject();
  node.Add("reactant", reactant);
  node.Add("partner", partner);
  node.Add("products", StringArray(products));
  node.Add("k_M-1s-1", k_M_inv_s_inv);
  return node;
}

DataNode MakeMoleculeNode(const std::string& name, double diffusion_m2_s, double radius_nm,
                          int charge)
{
  DataNode node = DataNode::MakeObject();
  node.Add("name", name);
  node.Add("D_m2s-1", diffusion_m2_s);
  node.Add("radius_nm", radius_nm);
  node.Add("charge", charge);
  return node;
}

DataNode BuildCatalog(const std::string& chemistryName)
{
  DataNode root = DataNode::MakeObject();
  root.Add("chemistry", chemistryName);
  root.Add("timeStepModel",
           G4EmParameters::Instance()->GetTimeStepModel() == G4ChemTimeStepModel::SBS ? "SBS"
                                                                                       : "IRT_syn");
  root.Add("geant4Version", VersionString());

  // Molecules, sorted by name.
  auto* moleculeTable = G4MoleculeTable::Instance();
  std::vector<const G4MolecularConfiguration*> configurations;
  auto confIterator = moleculeTable->GetConfigurationIterator();
  confIterator.reset();
  while (confIterator()) {
    configurations.push_back(confIterator.value());
  }
  std::sort(configurations.begin(), configurations.end(),
            [](const auto* a, const auto* b) { return a->GetName() < b->GetName(); });

  DataNode molecules = DataNode::MakeArray();
  DataNode dissociation = DataNode::MakeArray();
  for (const auto* conf : configurations) {
    molecules.Push(MakeMoleculeNode(conf->GetName(), conf->GetDiffusionCoefficient() / (m2 / s),
                                    conf->GetVanDerVaalsRadius() / nm, conf->GetCharge()));

    const auto* channels = conf->GetDefinition()->GetDecayChannels(conf);
    if (channels == nullptr) {
      continue;
    }
    DataNode channelArray = DataNode::MakeArray();
    for (const auto* channel : *channels) {
      std::vector<std::string> products;
      for (G4int i = 0; i < channel->GetNbProducts(); ++i) {
        products.push_back(channel->GetProduct(i)->GetName());
      }
      DataNode channelNode = DataNode::MakeObject();
      channelNode.Add("name", std::string(channel->GetName()));
      channelNode.Add("probability", channel->GetProbability());
      channelNode.Add("products", StringArray(products));
      channelNode.Add("displacementType", static_cast<int>(channel->GetDisplacementType()));
      channelArray.Push(channelNode);
    }
    DataNode state = DataNode::MakeObject();
    state.Add("state", std::string(conf->GetName()));
    state.Add("channels", channelArray);
    dissociation.Push(state);
  }
  root.Add("molecules", molecules);
  root.Add("dissociation", dissociation);

  // Bimolecular reactions, table order.
  DataNode reactions = DataNode::MakeArray();
  for (const auto* data :
       G4DNAMolecularReactionTable::GetReactionTable()->GetVectorOfReactionData()) {
    reactions.Push(MakeReactionNode(
      data->GetReactant1()->GetName(), data->GetReactant2()->GetName(), ProductNames(data),
      data->GetObservedReactionRateConstant() / kRateUnit, data->GetReactionType()));
  }
  root.Add("reactions", reactions);

  // Bulk reactions, sorted by (reactant, partner, products).
  using BulkKey = std::tuple<std::string, std::string, std::vector<std::string>>;
  std::vector<std::pair<BulkKey, double>> bulkEntries;
  auto* processTable = G4ProcessTable::GetProcessTable();
  auto iterator = moleculeTable->GetDefintionIterator();
  iterator.reset();
  while (iterator()) {
    auto* process = processTable->FindProcess("G4DNAScavengerProcess", iterator.value());
    auto* access = dynamic_cast<ScavengerReactionAccess*>(process);
    if (access == nullptr) {
      continue;
    }
    for (const auto& [mol, materialMap] : access->GetReactionMap()) {
      for (const auto& [material, data] : materialMap) {
        bulkEntries.push_back({BulkKey{mol->GetName(), material->GetName(), ProductNames(data)},
                               data->GetObservedReactionRateConstant() / kRateUnit});
      }
    }
  }
  std::sort(bulkEntries.begin(), bulkEntries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  DataNode bulk = DataNode::MakeArray();
  for (const auto& [key, rate] : bulkEntries) {
    bulk.Push(MakeBulkNode(std::get<0>(key), std::get<1>(key), std::get<2>(key), rate));
  }
  root.Add("bulk", bulk);
  return root;
}

void WriteCatalog(const G4String& filename, const std::string& chemistryName)
{
  std::ofstream out(filename);
  if (!out.is_open()) {
    G4Exception("CatalogExport::WriteCatalog", "CannotOpenFile", FatalException,
                (G4String("Could not open catalogue export file: ") + filename).c_str());
    return;
  }
  JsonWriter::Write(out, BuildCatalog(chemistryName));
  DnaLogger::Print(DnaLogger::Level::Info, "[CatalogExport] catalogue written to " + filename);
}
}  // namespace CatalogExport
