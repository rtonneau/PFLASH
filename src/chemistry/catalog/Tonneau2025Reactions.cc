/// \file Tonneau2025Reactions.cc
/// \brief Implementation of the Tonneau2025 Chemistry (see Tonneau2025Reactions.hh
/// for the routing, the reaction types and the open points)

#include "chemistry/catalog/Tonneau2025Reactions.hh"

#include "chemistry/ChemistryTypes.hh"

#include "chemistry/catalog/Tonneau2025Table.hh"

#include "G4DNAMolecularReactionTable.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <map>

namespace
{
// Configuration tags containing the UTF-8 degree sign (0xC2 0xB0), spelled
// from explicit bytes so they match the names stored by
// G4ChemDissociationChannels_option1 whatever this file's source encoding is.
// Duplicated locally (rather than shared with DnaChemistryList.cc) so this
// file has zero project-specific dependencies.
const G4String kOH = G4String("\xC2\xB0") + "OH";    // hydroxyl radical
const G4String kHO2 = G4String("HO2") + "\xC2\xB0";  // hydroperoxyl radical

// HO3 (extra molecule, ADR 0007): O3 analogues, docs/literature/ho3-parameters.md.
// The single place to change them.
const char* const kHO3 = "HO3";
const G4double kHO3Diffusion = 2.0e-9 * m2 / s;  // G4O3.cc
const G4double kHO3Radius = 0.20 * nm;           // O3 in G4ChemDissociationChannels_option1
const G4double kHO3Mass = 49.0053 * g / Avogadro * c_squared;

using Tonneau2025Reactions::Route;
using Tonneau2025Table::Reaction;
using Tonneau2025Table::Term;
using Tonneau2025Table::Unit;
using MolConf = const G4MolecularConfiguration*;

const char* const kCaller = "Tonneau2025Reactions";

MolConf Conf(const G4String& name)
{
  auto* p = G4MoleculeTable::Instance()->GetConfiguration(name, false);
  if (p == nullptr) {
    G4Exception(kCaller, "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}

/// Project names of the terms, each repeated `count` times, H2O dropped.
std::vector<std::string> Expand(const std::vector<Term>& terms)
{
  std::vector<std::string> names;
  for (const auto& t : terms) {
    const std::string name = Tonneau2025Reactions::SpeciesName(t.species);
    if (name.empty()) {
      continue;  // the solvent
    }
    for (int i = 0; i < t.count; ++i) {
      names.push_back(name);
    }
  }
  return names;
}

bool IsBufferIon(const std::string& name) { return name == "H3Op" || name == "OHm"; }

/// H3O+ and OH- made or consumed by a bulk reaction belong to the fixed buffer.
std::string ToBuffer(const std::string& name)
{
  return IsBufferIon(name) ? name + "(B)" : name;
}

std::vector<std::string> ToBuffer(std::vector<std::string> names)
{
  for (auto& n : names) {
    n = ToBuffer(n);
  }
  return names;
}

/// Rate constant in Geant4 internal units.
G4double Rate(const Reaction& r)
{
  switch (r.unit) {
    case Unit::SecondOrder:
      return r.k * (1e-3 * m3 / (mole * s));
    case Unit::FirstOrder:
      return r.k / s;
    case Unit::ZeroOrder:
      break;
  }
  G4Exception(kCaller, "UnsupportedUnit", FatalException,
              ("Row R" + std::to_string(r.id) + " has a zero-order rate constant; only the "
                                                 "buffer row R58 may, and it is not a reaction")
                .c_str());
  return 0.;
}

bool InTable(Route route)
{
  return route == Route::Table || route == Route::TableAndBulk || route == Route::WaterTable ||
         route == Route::BulkAndWaterTable;
}

bool InBulk(Route route)
{
  return route == Route::TableAndBulk || route == Route::Bulk ||
         route == Route::BulkAndWaterTable;
}

std::vector<Tonneau2025Reactions::Placement> MakePlacements()
{
  constexpr Route T = Route::Table;
  constexpr Route TB = Route::TableAndBulk;
  constexpr Route B = Route::Bulk;
  // {id, route, tableType, bulkType}; the per-row reasons are in the header.
  return {
      {1, T, 0, -1},   {2, T, 0, -1},   {3, T, 1, -1},   {4, T, 1, -1},   {5, T, 1, -1},
      {6, T, 1, -1},   {7, TB, 1, 0},   {8, T, 1, -1},   {9, T, 1, -1},   {10, T, 1, -1},
      {11, T, 1, -1},  {12, T, 1, -1},  {13, T, 0, -1},  {14, T, 1, -1},  {15, T, 1, -1},
      {16, T, 1, -1},  {17, T, 1, -1},  {18, T, 1, -1},  {19, T, 1, -1},  {20, T, 1, -1},
      {21, T, 1, -1},  {22, T, 1, -1},  {23, Route::WaterTable, 0, -1},   {24, T, 0, -1},
      {25, T, 1, -1},  {26, T, 0, -1},  {27, T, 1, -1},  {28, T, 1, -1},  {29, T, 1, -1},
      {30, T, 0, -1},  {31, TB, 1, 0},  {32, T, 1, -1},  {33, T, 1, -1},  {34, T, 1, -1},
      {35, T, 1, -1},  {36, T, 1, -1},  {37, T, 1, -1},  {38, T, 1, -1},  {39, T, 1, -1},
      {40, T, 1, -1},  {41, T, 0, -1},  {42, T, 1, -1},  {43, T, 1, -1},  {44, T, 1, -1},
      {45, T, 1, -1},  {46, T, 1, -1},  {47, TB, 1, 0},  {48, T, 1, -1},  {49, T, 1, -1},
      {50, T, 1, -1},  {51, T, 1, -1},  {52, T, 1, -1},  {53, B, -1, 0},  {54, TB, 0, 0},
      {55, B, -1, 0},  {56, TB, 1, 0},  {57, B, -1, 0},  {58, Route::Buffer, -1, -1},
      {59, TB, 0, 0},  {60, B, -1, 0},  {61, TB, 1, 0},  {62, B, -1, 0},  {63, TB, 1, 0},
      {64, B, -1, 6},  {65, TB, 1, 6},  {66, B, -1, 0},  {67, TB, 1, 0},  {68, TB, 1, 7},
      {69, B, -1, 7},  {70, TB, 1, 0},  {71, Route::BulkAndWaterTable, 0, 0},
      {72, TB, 1, 8},  {73, B, -1, 8},
  };
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const std::vector<Tonneau2025Reactions::Placement>& Tonneau2025Reactions::Placements()
{
  static const std::vector<Placement> placements = MakePlacements();
  return placements;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

std::string Tonneau2025Reactions::SpeciesName(const std::string& paperName)
{
  static const std::map<std::string, std::string> names = {
      {"e-aq", "e_aq"}, {"H.", "H"},     {"OH.", kOH},    {"OH-", "OHm"},   {"H3O+", "H3Op"},
      {"O-", "Om"},     {"O2-", "O2m"},  {"O3-", "O3m"},  {"HO2.", kHO2},   {"HO2-", "HO2m"},
      {"HO3", kHO3},    {"O3", "O3"},    {"O2", "O2"},    {"H2", "H2"},     {"H2O2", "H2O2"},
      {"H2O", ""},
  };
  const auto it = names.find(paperName);
  return it != names.end() ? it->second : "?" + paperName;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

std::vector<std::string> Tonneau2025Reactions::TypeOneSpecies()
{
  std::vector<std::string> species;
  for (const auto& r : Tonneau2025Table::Reactions()) {
    if (Placements()[static_cast<std::size_t>(r.id - 1)].tableType != 1) {
      continue;
    }
    for (const auto& name : Expand(r.reactants)) {
      species.push_back(name);
    }
  }
  std::sort(species.begin(), species.end());
  species.erase(std::unique(species.begin(), species.end()), species.end());
  return species;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void Tonneau2025Reactions::BuildTonneau2025Reactions(G4DNAMolecularReactionTable* reactionTable)
{
  // type 0: fully diffusion-controlled (the default). type 1: partially
  // diffusion-controlled, SetReactionType(1) unless
  // ChemistryTypes::PartialReactionsEnabled() is false (SBS: stays type 0) -- the IRT stepper then samples
  // an activation step with the vdW reaction radius, so both reactants need one.
  auto add = [reactionTable](const Reaction& r, MolConf a, MolConf b,
                             const std::vector<std::string>& products, int type) {
    auto* rd = new G4DNAMolecularReactionData(Rate(r), a, b);
    for (const auto& p : products) {
      rd->AddProduct(Conf(p));
    }
    if (type == 1 && ChemistryTypes::PartialReactionsEnabled()) {
      for (MolConf reactant : {a, b}) {
        if (reactant->GetVanDerVaalsRadius() <= 0.) {
          G4Exception(kCaller, "MissingVdWRadius", FatalException,
                      ("R" + std::to_string(r.id) + ": " + reactant->GetName() +
                       " has no vdW radius, needed by a partially diffusion-controlled "
                       "(type 1) reaction")
                        .c_str());
        }
      }
      rd->SetReactionType(1);
    }
    reactionTable->SetReaction(rd);
  };

  // Table 2 order. R23 comes before R71, so the reaction table's pair lookup
  // for H + H2O returns R71, the entry the bulk-reaction list shares.
  for (const auto& r : Tonneau2025Table::Reactions()) {
    const auto& placement = Placements()[static_cast<std::size_t>(r.id - 1)];
    if (!InTable(placement.route)) {
      continue;
    }
    const auto reactants = Expand(r.reactants);
    if (placement.route == Route::WaterTable || placement.route == Route::BulkAndWaterTable) {
      if (reactants.size() != 1) {
        G4Exception(kCaller, "BadRouting", FatalException,
                    ("R" + std::to_string(r.id) + " is routed against H2O but has " +
                     std::to_string(reactants.size()) + " solute reactants")
                      .c_str());
      }
      // Same products as the bulk entry (buffer ions), so the driver shares it.
      add(r, Conf(reactants[0]), Conf("H2O"), ToBuffer(Expand(r.products)), placement.tableType);
      continue;
    }
    if (reactants.size() != 2) {
      G4Exception(kCaller, "BadRouting", FatalException,
                  ("R" + std::to_string(r.id) + " is routed to the reaction table but has " +
                   std::to_string(reactants.size()) + " solute reactants")
                    .c_str());
    }
    add(r, Conf(reactants[0]), Conf(reactants[1]), Expand(r.products), placement.tableType);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

ChemistryTypes::BulkReactionList Tonneau2025Reactions::BuildTonneau2025BulkReactions()
{
  ChemistryTypes::BulkReactionList list;
  auto reactionsOf = [&list](const std::string& molecule) -> std::vector<ChemistryTypes::BulkReaction>& {
    for (auto& entry : list) {
      if (entry.molecule == molecule) {
        return entry.reactions;
      }
    }
    list.push_back({molecule, {}});
    return list.back().reactions;
  };

  for (const auto& r : Tonneau2025Table::Reactions()) {
    const auto& placement = Placements()[static_cast<std::size_t>(r.id - 1)];
    if (!InBulk(placement.route)) {
      continue;
    }
    const auto reactants = Expand(r.reactants);
    const auto products = ToBuffer(Expand(r.products));
    const G4double rate = Rate(r);
    const int type = placement.bulkType;

    if (reactants.size() == 1) {
      // Water pseudo-first-order reaction or unimolecular decay: Geant4 takes
      // an "H2O" partner's rate as the first-order rate.
      reactionsOf(reactants[0]).push_back({"H2O", rate, products, type});
      continue;
    }

    bool placed = false;
    if (reactants.size() == 2) {
      // Acid-base: the other reactant against the buffer ion (both ways for R59).
      for (std::size_t i = 0; i < 2; ++i) {
        if (IsBufferIon(reactants[i])) {
          reactionsOf(reactants[1 - i]).push_back({ToBuffer(reactants[i]), rate, products, type});
          placed = true;
        }
      }
      // Dissolved O2, set by /chem/env/scavenger O2.
      for (std::size_t i = 0; i < 2 && !placed; ++i) {
        if (reactants[i] == "O2") {
          reactionsOf(reactants[1 - i]).push_back({"O2", rate, products, type});
          placed = true;
        }
      }
    }
    if (!placed) {
      G4Exception(kCaller, "BadRouting", FatalException,
                  ("R" + std::to_string(r.id) +
                   " is routed to the bulk list but has no H2O, buffer ion or O2 partner")
                    .c_str());
    }
  }
  return list;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void Tonneau2025Reactions::ConstructTonneau2025Molecules()
{
  auto* molTable = G4MoleculeTable::Instance();
  if (molTable->GetConfiguration(kHO3, false) != nullptr) {
    return;  // already created
  }

  // `new G4MoleculeDefinition` is not idempotent: reuse a definition that a
  // previous call (or anyone else) registered under this name.
  G4MoleculeDefinition* definition = molTable->GetMoleculeDefinition(kHO3, false);
  if (definition == nullptr) {
    // Neutral radical, 4 atoms; built like G4HO2 / G4O3.
    definition = new G4MoleculeDefinition(kHO3, kHO3Mass, kHO3Diffusion, 0, 0, kHO3Radius, 4);
    definition->SetLevelOccupation(0);
    definition->SetFormatedName("HO_{3}");
  }

  // The definition's default configuration, whose user ID is the definition's
  // name ("HO3"). G4DNAChemistryManager later calls
  // G4MoleculeTable::PrepareMolecularConfiguration, which gets or creates
  // exactly this configuration for every definition; a configuration made
  // with CreateConfiguration("HO3", ...) is not found by that lookup, so a
  // second one would claim the user ID "HO3" (fatal CONF_ALREADY_RECORDED).
  auto* configuration = G4MolecularConfiguration::GetOrCreateMolecularConfiguration(definition);
  configuration->SetDiffusionCoefficient(kHO3Diffusion);
  configuration->SetVanDerVaalsRadius(kHO3Radius);
}
