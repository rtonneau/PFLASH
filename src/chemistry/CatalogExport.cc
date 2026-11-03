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
#include <set>
#include <tuple>
#include <utility>

namespace
{
const G4double kSecondOrderUnit = 1e-3 * m3 / (mole * s);  // M^-1 s^-1
const G4double kFirstOrderUnit = 1. / s;                     // s^-1

// Geant4 uses an H2O-partner rate as a first-order rate (G4DNAScavengerProcess:
// propensity = rate; G4DNAGillespieDirectMethod::FindScavenging: Avogadro * V
// water molecules per node, so propensity = rate * n). Every other pair is second order.
G4int ReactionOrder(const G4MolecularConfiguration* a, const G4MolecularConfiguration* b)
{
  const auto* water = G4MoleculeTable::Instance()->GetConfiguration("H2O", false);
  return (water != nullptr && (a == water || b == water)) ? 1 : 2;
}

G4double RateInExportUnit(G4double rate, G4int order)
{
  return rate / (order == 1 ? kFirstOrderUnit : kSecondOrderUnit);
}

void AddRate(DataNode& node, double k, int order)
{
  node.Add("order", order);
  node.Add(CatalogExport::RateKey(order), k);
}

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
    names.push_back(CatalogExport::SpeciesId(data->GetProduct(i)));
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
std::string RateKey(int order)
{
  return order == 1 ? "k_s-1" : "k_M-1s-1";
}

DataNode MakeReactionNode(const std::string& reactant1, const std::string& reactant2,
                          const std::vector<std::string>& products, double k, int order,
                          int type, bool mesoOnly)
{
  DataNode node = DataNode::MakeObject();
  node.Add("reactants", StringArray({reactant1, reactant2}));
  node.Add("products", StringArray(products));
  AddRate(node, k, order);
  node.Add("type", type);
  node.Add("mesoOnly", mesoOnly);
  return node;
}

std::string SpeciesId(const G4MolecularConfiguration* configuration)
{
  const G4String& userId = configuration->GetUserID();
  return userId.empty() ? std::string(configuration->GetName()) : std::string(userId);
}

DataNode MakeBulkNode(const std::string& reactant, const std::string& partner,
                      const std::vector<std::string>& products, double k, int order)
{
  DataNode node = DataNode::MakeObject();
  node.Add("reactant", reactant);
  node.Add("partner", partner);
  node.Add("products", StringArray(products));
  AddRate(node, k, order);
  return node;
}

DataNode MakeMoleculeNode(const std::string& name, double diffusion_m2_s, double radius_nm,
                          int charge, const std::string& configuration)
{
  DataNode node = DataNode::MakeObject();
  node.Add("name", name);
  node.Add("D_m2s-1", diffusion_m2_s);
  node.Add("radius_nm", radius_nm);
  node.Add("charge", charge);
  node.Add("configuration", configuration);
  return node;
}

DataNode BuildCatalog(const std::string& chemistryName,
                      const std::set<const G4DNAMolecularReactionData*>& mesoOnlyReactions)
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
            [](const auto* a, const auto* b) { return SpeciesId(a) < SpeciesId(b); });

  DataNode molecules = DataNode::MakeArray();
  DataNode dissociation = DataNode::MakeArray();
  for (const auto* conf : configurations) {
    molecules.Push(MakeMoleculeNode(SpeciesId(conf), conf->GetDiffusionCoefficient() / (m2 / s),
                                    conf->GetVanDerVaalsRadius() / nm, conf->GetCharge(),
                                    std::string(conf->GetName())));

    const auto* channels = conf->GetDefinition()->GetDecayChannels(conf);
    if (channels == nullptr) {
      continue;
    }
    DataNode channelArray = DataNode::MakeArray();
    for (const auto* channel : *channels) {
      std::vector<std::string> products;
      for (G4int i = 0; i < channel->GetNbProducts(); ++i) {
        products.push_back(SpeciesId(channel->GetProduct(i)));
      }
      DataNode channelNode = DataNode::MakeObject();
      channelNode.Add("name", std::string(channel->GetName()));
      channelNode.Add("probability", channel->GetProbability());
      channelNode.Add("products", StringArray(products));
      channelNode.Add("displacementType", static_cast<int>(channel->GetDisplacementType()));
      channelArray.Push(channelNode);
    }
    DataNode state = DataNode::MakeObject();
    state.Add("state", SpeciesId(conf));
    state.Add("channels", channelArray);
    dissociation.Push(state);
  }
  root.Add("molecules", molecules);
  root.Add("dissociation", dissociation);

  // Bimolecular reactions, table order.
  DataNode reactions = DataNode::MakeArray();
  for (const auto* data :
       G4DNAMolecularReactionTable::GetReactionTable()->GetVectorOfReactionData()) {
    const G4int order = ReactionOrder(data->GetReactant1(), data->GetReactant2());
    reactions.Push(MakeReactionNode(
      SpeciesId(data->GetReactant1()), SpeciesId(data->GetReactant2()), ProductNames(data),
      RateInExportUnit(data->GetObservedReactionRateConstant(), order), order,
      data->GetReactionType(), mesoOnlyReactions.count(data) > 0));
  }
  root.Add("reactions", reactions);

  // Bulk reactions, sorted by (reactant, partner, products).
  using BulkKey = std::tuple<std::string, std::string, std::vector<std::string>>;
  std::vector<std::pair<BulkKey, std::pair<double, int>>> bulkEntries;
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
        const G4int order = ReactionOrder(mol, material);
        bulkEntries.push_back(
          {BulkKey{SpeciesId(mol), SpeciesId(material), ProductNames(data)},
           {RateInExportUnit(data->GetObservedReactionRateConstant(), order), order}});
      }
    }
  }
  std::sort(bulkEntries.begin(), bulkEntries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  DataNode bulk = DataNode::MakeArray();
  for (const auto& [key, rate] : bulkEntries) {
    bulk.Push(
      MakeBulkNode(std::get<0>(key), std::get<1>(key), std::get<2>(key), rate.first, rate.second));
  }
  root.Add("bulk", bulk);
  return root;
}

void WriteCatalog(const G4String& filename, const std::string& chemistryName,
                  const std::set<const G4DNAMolecularReactionData*>& mesoOnlyReactions)
{
  std::ofstream out(filename);
  if (!out.is_open()) {
    G4Exception("CatalogExport::WriteCatalog", "CannotOpenFile", FatalException,
                (G4String("Could not open catalogue export file: ") + filename).c_str());
    return;
  }
  JsonWriter::Write(out, BuildCatalog(chemistryName, mesoOnlyReactions));
  DnaLogger::Print(DnaLogger::Level::Info, "[CatalogExport] catalogue written to " + filename);
}
}  // namespace CatalogExport
