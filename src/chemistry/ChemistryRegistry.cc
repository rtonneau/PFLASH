/// \file ChemistryRegistry.cc
#include "chemistry/ChemistryRegistry.hh"

#include <algorithm>
#include <cctype>

namespace
{
using ChemistryRegistry::Chemistry;

struct State
{
  std::vector<Chemistry> entries;
  std::string selected;  // canonical name; empty = nothing picked yet
};

State& Instance()
{
  static State state;
  return state;
}

std::string Lower(const std::string& text)
{
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

const Chemistry* Find(const std::string& name)
{
  const std::string wanted = Lower(name);
  for (const auto& entry : Instance().entries) {
    if (Lower(entry.name) == wanted) {
      return &entry;
    }
  }
  return nullptr;
}

std::string JoinNames()
{
  std::string joined;
  for (const auto& entry : Instance().entries) {
    joined += (joined.empty() ? "" : ", ") + entry.name;
  }
  return joined.empty() ? "(none registered)" : joined;
}
}  // namespace

const char* const ChemistryRegistry::kDefaultName = "PureWater";

bool ChemistryRegistry::Register(const Chemistry& chemistry, std::string& err)
{
  if (chemistry.name.empty()) {
    err = "Chemistry name must not be empty.";
    return false;
  }
  if (chemistry.buildReactions == nullptr || chemistry.buildBulkReactions == nullptr) {
    err = "Chemistry '" + chemistry.name + "' has a null builder.";
    return false;
  }
  if (Find(chemistry.name) != nullptr) {
    err = "Chemistry '" + chemistry.name + "' is already registered.";
    return false;
  }
  Instance().entries.push_back(chemistry);
  return true;
}

bool ChemistryRegistry::Select(const std::string& name, std::string& err)
{
  const Chemistry* found = Find(name);
  if (found == nullptr) {
    err = "Unknown chemistry '" + name + "'. Valid names: " + JoinNames() + ".";
    return false;
  }
  auto& state = Instance();
  if (!state.selected.empty() && state.selected != found->name) {
    err = "Chemistry already set to '" + state.selected + "'; refusing to change it to '" +
          found->name + "'.";
    return false;
  }
  state.selected = found->name;
  return true;
}

const ChemistryRegistry::Chemistry* ChemistryRegistry::Selected()
{
  const auto& state = Instance();
  return Find(state.selected.empty() ? std::string(kDefaultName) : state.selected);
}

std::vector<std::string> ChemistryRegistry::Names()
{
  std::vector<std::string> names;
  for (const auto& entry : Instance().entries) {
    names.push_back(entry.name);
  }
  return names;
}

void ChemistryRegistry::ResetForTesting()
{
  Instance() = State{};
}
