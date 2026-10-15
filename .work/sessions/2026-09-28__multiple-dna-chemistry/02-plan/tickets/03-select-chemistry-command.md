# Ticket 03: select-chemistry-command

**Acceptance Criteria:**
- [ ] `BuiltInChemistries::Register()` is idempotent and registers `PureWater` under `ChemistryRegistry::kDefaultName`.
- [ ] `/chem/select <name>` (PreInit only) and `/chem/list` exist and appear under `/control/manual /chem/`.
- [ ] `DnaChemistryList` builds its reaction table and acid-base processes from `ChemistryRegistry::Selected()`; the Info log line names the Chemistry.
- [ ] With no `/chem/select`, and with `/chem/select purewater` (lower case), the sorted reaction dump equals the ticket-02 baseline and `PhysicsInteractions.csv`/`EnergyDeposit.Txt` are unchanged.
- [ ] `/chem/select Nope` before `/run/initialize` is fatal and the message lists `PureWater`.
- [ ] `/chem/list` prints the registered names and marks the default.
- [ ] All unit tests still pass in `build-ninja`.

**Files to Touch:**
- `header/BuiltInChemistries.hh` (create)
- `src/BuiltInChemistries.cc` (create)
- `header/ChemistrySelectMessenger.hh` (create)
- `src/ChemistrySelectMessenger.cc` (create)
- `header/DnaChemistryList.hh`
- `src/DnaChemistryList.cc`

**Verification Step:**

Run (scratch macros in `build/macro/`: `gps_dump.in` from ticket 02; `gps_select_lower.in` is `gps_dump.in` with `/chem/select purewater` inserted before `/run/initialize`; `gps_select_unknown.in` is `/chem/select Nope` then `/run/initialize`; `gps_list.in` is `/chem/list` then `/run/initialize`):
```bash
cd build
S=C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry
./sim.exe gps_dump.in --dir $S/after03_default > after03_default.log 2>&1
./sim.exe gps_select_lower.in --dir $S/after03_lower > after03_lower.log 2>&1
diff <(sort $S/baseline/ReactionTable.txt) <(sort $S/after03_default/ReactionTable.txt) && echo DEFAULT_SAME
diff <(sort $S/baseline/ReactionTable.txt) <(sort $S/after03_lower/ReactionTable.txt) && echo LOWER_SAME
diff $S/baseline/PhysicsInteractions.csv $S/after03_default/PhysicsInteractions.csv && echo PHYS_SAME
./sim.exe gps_select_unknown.in > unknown.log 2>&1; grep -c "Unknown chemistry 'Nope'.*PureWater" unknown.log
./sim.exe gps_list.in > list.log 2>&1; grep "PureWater" list.log
grep -c "chemistry = PureWater" after03_default.log
```
Then in `build-ninja`:
```powershell
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build-ninja"
ctest --test-dir build-ninja --output-on-failure
```

Expected:
`DEFAULT_SAME`, `LOWER_SAME`, `PHYS_SAME`, then `1` for the unknown-name message, a `/chem/list` line containing `PureWater` and `(default)`, `1` for the Info line, and all CTest tests `Passed` with none `Not Run`. Run the simulations with `run_in_background`; the unknown-name and list runs stop before any event, so they finish quickly.

**Notes:**

`header/BuiltInChemistries.hh`:
```cpp
/// \file BuiltInChemistries.hh
/// \brief Registers the Chemistries compiled into this project.
#ifndef BuiltInChemistries_h
#define BuiltInChemistries_h 1

namespace BuiltInChemistries
{
  /// Registers every built-in Chemistry with ChemistryRegistry. Safe to call
  /// more than once; only the first call registers. Raises a fatal G4Exception
  /// if a registration is rejected (a programming error, e.g. a duplicate name).
  void Register();
}  // namespace BuiltInChemistries

#endif  // BuiltInChemistries_h
```

`src/BuiltInChemistries.cc`:
```cpp
/// \file BuiltInChemistries.cc
#include "BuiltInChemistries.hh"

#include "ChemistryRegistry.hh"
#include "PureWaterReactions.hh"

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
       &PureWaterReactions::BuildPureWaterAcidBase});
}
```

`header/ChemistrySelectMessenger.hh` (same style as `OutputDirMessenger`):
```cpp
/// \file ChemistrySelectMessenger.hh
/// \brief Macro commands to choose and list Chemistries.
///   /chem/select <name>   (PreInit only) pick the Chemistry for this process
///   /chem/list            print the registered Chemistries
/// /chem/ already exists (created by G4DNAChemistryManager), so this is a plain
/// G4UImessenger rather than a G4GenericMessenger, which would create a second
/// /chem/ directory. Construct it after G4DNAChemistryManager::Instance().
#ifndef ChemistrySelectMessenger_h
#define ChemistrySelectMessenger_h 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;
class G4UIcmdWithoutParameter;

class ChemistrySelectMessenger : public G4UImessenger
{
public:
  ChemistrySelectMessenger();
  ~ChemistrySelectMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  G4String GetCurrentValue(G4UIcommand* command) override;

private:
  G4UIcmdWithAString* fpSelectCmd;
  G4UIcmdWithoutParameter* fpListCmd;
};

#endif  // ChemistrySelectMessenger_h
```

`src/ChemistrySelectMessenger.cc`:
```cpp
/// \file ChemistrySelectMessenger.cc
#include "ChemistrySelectMessenger.hh"

#include "ChemistryRegistry.hh"

#include "G4ApplicationState.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"

#include <string>

ChemistrySelectMessenger::ChemistrySelectMessenger()
{
  fpSelectCmd = new G4UIcmdWithAString("/chem/select", this);
  fpSelectCmd->SetGuidance(
    "Select the Chemistry (reaction network) for this run, by name, case-insensitive. "
    "Use /chem/list to see the names. Default: PureWater. Must be issued before "
    "/run/initialize. Issuing a different name twice, or an unknown name, is fatal.");
  fpSelectCmd->SetParameterName("name", false);
  fpSelectCmd->AvailableForStates(G4State_PreInit);

  fpListCmd = new G4UIcmdWithoutParameter("/chem/list", this);
  fpListCmd->SetGuidance("Print the available Chemistries; the default is marked.");
  fpListCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

ChemistrySelectMessenger::~ChemistrySelectMessenger()
{
  delete fpSelectCmd;
  delete fpListCmd;
}

void ChemistrySelectMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command == fpSelectCmd) {
    std::string err;
    if (!ChemistryRegistry::Select(newValue, err)) {
      G4Exception("ChemistrySelectMessenger::SetNewValue", "InvalidChemistrySelection",
                  FatalException, err.c_str());
    }
  }
  else if (command == fpListCmd) {
    G4cout << "Available chemistries:";
    for (const auto& name : ChemistryRegistry::Names()) {
      G4cout << " " << name << (name == ChemistryRegistry::kDefaultName ? " (default)" : "");
    }
    G4cout << G4endl;
  }
}

G4String ChemistrySelectMessenger::GetCurrentValue(G4UIcommand* command)
{
  if (command == fpSelectCmd) {
    const auto* selected = ChemistryRegistry::Selected();
    return selected != nullptr ? G4String(selected->name) : G4String("");
  }
  return "";
}
```

`header/DnaChemistryList.hh`: forward-declare `class ChemistrySelectMessenger;` and add the member `std::unique_ptr<ChemistrySelectMessenger> fSelectMessenger;`. Because `unique_ptr` needs the complete type where the destructor is defined, the class currently uses `~DnaChemistryList() override = default;` in the header. Change it to a declaration and define `DnaChemistryList::~DnaChemistryList() = default;` in the `.cc`, which includes `ChemistrySelectMessenger.hh`. Add a private helper `const ChemistryRegistry::Chemistry* SelectedChemistry(const G4String& caller) const;` (forward-declare `namespace ChemistryRegistry { struct Chemistry; }`).

`src/DnaChemistryList.cc`:
- Includes: `BuiltInChemistries.hh`, `ChemistryRegistry.hh`, `ChemistrySelectMessenger.hh`; drop `PureWaterReactions.hh` if nothing else uses it.
- Constructor, first statements: call `BuiltInChemistries::Register();` first, then the existing `SetChemistryList(this)`, then `fSelectMessenger = std::make_unique<ChemistrySelectMessenger>();` (after `G4DNAChemistryManager::Instance()` exists, so `/chem/` is there).
- Add the helper, fatal `G4Exception("DnaChemistryList::<caller>", "NoChemistry", FatalException, ...)` when `Selected()` is null:
```cpp
const ChemistryRegistry::Chemistry* DnaChemistryList::SelectedChemistry(const G4String& caller) const
{
  const auto* chemistry = ChemistryRegistry::Selected();
  if (chemistry == nullptr) {
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "NoChemistry", FatalException,
                "No chemistry is selected and the default is not registered.");
  }
  return chemistry;
}
```
- `ConstructReactionTable`: replace `PureWaterReactions::BuildPureWaterReactions(reactionTable);` with `SelectedChemistry("ConstructReactionTable")->buildReactions(reactionTable);` and change the Info line to `"[DnaChemistryList] chemistry = " + name + ", reaction table constructed"`.
- `ConstructProcess`: replace the ticket-02 call with `RegisterAcidBaseScavengerProcesses(*chemWorld->GetChemistryBoundary(), SelectedChemistry("ConstructProcess")->buildAcidBase());`.
- Update the file and class comments: the reaction content comes from the selected Chemistry; `PureWater` is the default (see `docs/adr/0002-named-chemistries.md`).

Threading: `ConstructReactionTable` runs on the master, and `ConstructProcess` can run per worker thread. Both only read the registry, which nothing writes after the constructor.

Also build `sim` in `build` (RelWithDebInfo) for the macros, and `build-ninja` for CTest; with `CONFIGURE_DEPENDS` the new `.cc` files are picked up by the next build. Commit message: `feat: select the chemistry with /chem/select`.
