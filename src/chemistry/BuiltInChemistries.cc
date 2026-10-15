/// \file BuiltInChemistries.cc
#include "chemistry/BuiltInChemistries.hh"

#include "chemistry/catalog/BoscoloChemReactions.hh"
#include "chemistry/ChemistryRegistry.hh"
#include "chemistry/catalog/Farokhi2023Reactions.hh"
#include "chemistry/catalog/PureWaterReactions.hh"
#include "chemistry/catalog/Tonneau2025Reactions.hh"

#include "globals.hh"

#include <string>

void BuiltInChemistries::Register()
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  auto add = [](const ChemistryRegistry::Chemistry& chemistry) {
    std::string err;
    if (!ChemistryRegistry::Register(chemistry, err)) {
      G4Exception("BuiltInChemistries::Register", "InvalidChemistry", FatalException,
                  err.c_str());
    }
  };

  add({ChemistryRegistry::kDefaultName, &PureWaterReactions::BuildPureWaterReactions,
       &PureWaterReactions::BuildPureWaterBulkReactions});
  add({"BoscoloChem", &BoscoloChemReactions::BuildBoscoloChemReactions,
       &BoscoloChemReactions::BuildBoscoloChemBulkReactions});
  add({"Tonneau2025", &Tonneau2025Reactions::BuildTonneau2025Reactions,
       &Tonneau2025Reactions::BuildTonneau2025BulkReactions,
       &Tonneau2025Reactions::ConstructTonneau2025Molecules});
  add({"Farokhi2023", &Farokhi2023Reactions::BuildFarokhiReactions,
       &Farokhi2023Reactions::BuildFarokhiBulkReactions});
}
