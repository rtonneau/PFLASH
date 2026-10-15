# Ticket 01: chemistry-registry-core

**Acceptance Criteria:**
- [ ] `header/ChemistryTypes.hh`, `header/ChemistryRegistry.hh` and `src/ChemistryRegistry.cc` exist, use only the standard library plus a forward declaration of `G4DNAMolecularReactionTable`, and match the design lock in the plan.
- [ ] `Register` rejects an empty name, a null builder, and a duplicate name (case-insensitive), filling `err`.
- [ ] `Select` matches case-insensitively and stores the canonical spelling; an unknown name returns false with `Unknown chemistry '<name>'. Valid names: <A>, <B>.`; a second `Select` with a different name returns false; the same name again returns true.
- [ ] `Selected()` returns the picked entry, else the entry named `kDefaultName`, else `nullptr` when the default is not registered.
- [ ] `ChemistryRegistryTest` is wired into CMake and passes from `build-ninja`.

**Files to Touch:**
- `header/ChemistryTypes.hh` (create)
- `header/ChemistryRegistry.hh` (create)
- `src/ChemistryRegistry.cc` (create)
- `test/ChemistryRegistryTest.cc` (create)
- `CMakeLists.txt`

**Verification Step:**

Run:
```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build-ninja --target ChemistryRegistryTest"
ctest --test-dir build-ninja -R ChemistryRegistryTest --output-on-failure
```

Expected:
`100% tests passed, 1 tests passed out of 1`. Before the implementation exists, the first build fails to link (no `ChemistryRegistry.cc`), so write the header and an empty-bodied `.cc` first and watch an assertion fail, then implement.

**Notes:**

Follow test-first: write the test and headers, stub the `.cc` so it links, see an `assert` fail, then implement.

`header/ChemistryTypes.hh`:
```cpp
/// \file ChemistryTypes.hh
/// \brief Plain data types shared by every Chemistry file. Standard library only.
#ifndef ChemistryTypes_h
#define ChemistryTypes_h 1

#include <string>
#include <vector>

namespace ChemistryTypes
{
  /// One first-order or pseudo-first-order acid-base reaction of a tracked
  /// molecule against a partner (a bulk species such as "H3Op(B)" or "H2O").
  /// rate is already in Geant4 internal units. reactionType 0 = leave unset.
  struct AcidBaseReaction
  {
    std::string partner;
    double rate;
    std::vector<std::string> products;
    int reactionType = 0;
  };

  /// All acid-base reactions registered as one G4DNAScavengerProcess on `molecule`.
  struct AcidBaseEntry
  {
    std::string molecule;
    std::vector<AcidBaseReaction> reactions;
  };

  /// May be empty: a Chemistry without the acid-base buffer.
  using AcidBaseList = std::vector<AcidBaseEntry>;
}  // namespace ChemistryTypes

#endif  // ChemistryTypes_h
```

`header/ChemistryRegistry.hh`:
```cpp
/// \file ChemistryRegistry.hh
/// \brief Name -> Chemistry registry and the once-per-process selection.
///
/// Pure logic: no Geant4 kernel, no logging, no process exit. Portable (needs
/// ChemistryTypes.hh only). Not thread-synchronized: Register/Select run from
/// the master thread before /run/initialize; afterwards everything is read-only.
#ifndef ChemistryRegistry_h
#define ChemistryRegistry_h 1

#include "ChemistryTypes.hh"

#include <string>
#include <vector>

class G4DNAMolecularReactionTable;

namespace ChemistryRegistry
{
  struct Chemistry
  {
    std::string name;
    void (*buildReactions)(G4DNAMolecularReactionTable*);
    ChemistryTypes::AcidBaseList (*buildAcidBase)();
  };

  /// Chemistry used when no /chem/select was issued.
  extern const char* const kDefaultName;

  /// Adds a Chemistry. False + err for an empty name, a null builder, or a
  /// name already registered (compared case-insensitively).
  bool Register(const Chemistry& chemistry, std::string& err);

  /// Picks the Chemistry, matching name case-insensitively. False + err if the
  /// name is unknown (err lists the valid names) or if a different Chemistry was
  /// already picked. Picking the same one again returns true and changes nothing.
  bool Select(const std::string& name, std::string& err);

  /// The picked Chemistry, else the entry named kDefaultName, else nullptr.
  const Chemistry* Selected();

  /// Registered names, in registration order.
  std::vector<std::string> Names();

  /// Clears every registration and the selection. Tests only.
  void ResetForTesting();
}  // namespace ChemistryRegistry

#endif  // ChemistryRegistry_h
```

`src/ChemistryRegistry.cc`:
```cpp
/// \file ChemistryRegistry.cc
#include "ChemistryRegistry.hh"

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
  if (chemistry.buildReactions == nullptr || chemistry.buildAcidBase == nullptr) {
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
```

`test/ChemistryRegistryTest.cc` (plain `assert`, same style as the other tests; `main()` starts with the CRT block):
```cpp
/// \file ChemistryRegistryTest.cc
/// \brief Plain-assert unit tests for ChemistryRegistry (no Geant4 runtime).
#include "ChemistryRegistry.hh"

#include <cassert>
#include <iostream>
#include <string>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
void DummyReactions(G4DNAMolecularReactionTable*) {}
ChemistryTypes::AcidBaseList DummyAcidBase() { return {}; }

ChemistryRegistry::Chemistry Make(const std::string& name)
{
  return {name, &DummyReactions, &DummyAcidBase};
}

void MustRegister(const std::string& name)
{
  std::string err;
  const bool ok = ChemistryRegistry::Register(Make(name), err);
  assert(ok);
  (void)ok;
}

void ResetWithTwo()
{
  ChemistryRegistry::ResetForTesting();
  MustRegister("PureWater");
  MustRegister("BoscoloChem");
}

bool Contains(const std::string& text, const std::string& part)
{
  return text.find(part) != std::string::npos;
}
}  // namespace

static void TestRegisterRejectsBadInput()
{
  ChemistryRegistry::ResetForTesting();
  std::string err;
  assert(!ChemistryRegistry::Register(Make(""), err));
  assert(!err.empty());
  err.clear();
  assert(!ChemistryRegistry::Register({"NoBuilders", nullptr, nullptr}, err));
  assert(Contains(err, "null builder"));
  MustRegister("PureWater");
  err.clear();
  assert(!ChemistryRegistry::Register(Make("purewater"), err));
  assert(Contains(err, "already registered"));
}

static void TestNamesKeepRegistrationOrder()
{
  ResetWithTwo();
  const auto names = ChemistryRegistry::Names();
  assert(names.size() == 2);
  assert(names[0] == "PureWater");
  assert(names[1] == "BoscoloChem");
}

static void TestSelectedFallsBackToDefault()
{
  ChemistryRegistry::ResetForTesting();
  assert(ChemistryRegistry::Selected() == nullptr);
  ResetWithTwo();
  assert(ChemistryRegistry::Selected() != nullptr);
  assert(ChemistryRegistry::Selected()->name == ChemistryRegistry::kDefaultName);
}

static void TestSelectIsCaseInsensitiveAndKeepsCanonicalName()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("boscolochem", err));
  assert(ChemistryRegistry::Selected()->name == "BoscoloChem");
}

static void TestSelectUnknownListsValidNames()
{
  ResetWithTwo();
  std::string err;
  assert(!ChemistryRegistry::Select("Nope", err));
  assert(Contains(err, "Unknown chemistry 'Nope'"));
  assert(Contains(err, "PureWater"));
  assert(Contains(err, "BoscoloChem"));
  assert(ChemistryRegistry::Selected()->name == "PureWater");  // unchanged
}

static void TestSelectSameNameTwiceIsNoOp()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("BoscoloChem", err));
  assert(ChemistryRegistry::Select("BOSCOLOCHEM", err));
  assert(ChemistryRegistry::Selected()->name == "BoscoloChem");
}

static void TestSelectDifferentNameConflicts()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("PureWater", err));  // explicit default still counts
  assert(!ChemistryRegistry::Select("BoscoloChem", err));
  assert(Contains(err, "already set to 'PureWater'"));
  assert(ChemistryRegistry::Selected()->name == "PureWater");
}

static void TestResetClearsSelectionAndEntries()
{
  ResetWithTwo();
  std::string err;
  assert(ChemistryRegistry::Select("BoscoloChem", err));
  ChemistryRegistry::ResetForTesting();
  assert(ChemistryRegistry::Names().empty());
  assert(ChemistryRegistry::Selected() == nullptr);
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestRegisterRejectsBadInput();
  TestNamesKeepRegistrationOrder();
  TestSelectedFallsBackToDefault();
  TestSelectIsCaseInsensitiveAndKeepsCanonicalName();
  TestSelectUnknownListsValidNames();
  TestSelectSameNameTwiceIsNoOp();
  TestSelectDifferentNameConflicts();
  TestResetClearsSelectionAndEntries();
  std::cout << "ChemistryRegistryTest: all tests passed\n";
  return 0;
}
```

`CMakeLists.txt`, next to the other test blocks (before the "Copy macro files" block):
```cmake
# Standalone unit test for ChemistryRegistry (pure name lookup/selection logic,
# no Geant4 kernel dependency -- it only stores builder function pointers).
add_executable(ChemistryRegistryTest test/ChemistryRegistryTest.cc src/ChemistryRegistry.cc)
target_link_libraries(ChemistryRegistryTest ${Geant4_LIBRARIES})
target_include_directories(ChemistryRegistryTest PRIVATE ${project_include_dirs})
add_test(NAME ChemistryRegistryTest COMMAND ChemistryRegistryTest)
```

Commit message: `feat: add portable ChemistryRegistry with unit test`.
