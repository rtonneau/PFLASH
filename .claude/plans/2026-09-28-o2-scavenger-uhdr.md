# Dissolved-O2 Bulk Scavenger (UHDR layout) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. In this project the tasks are shipped as gps tickets (`/gps ship` or `/gps ticket N`), one commit each.

**Goal:** Make dissolved O2 react as a bulk scavenger, as it does in the Geant4-DNA `UHDR` example, and replace the no-op `/chem/env/O2` with a generic `/chem/env/scavenger <species> <value> <unit>`.

**Architecture:** The scavenger concentration belongs to the environment: `DnaChemistryWorld` stores it and puts it into `fpChemicalComponent`. The reactions against it belong to the selected Chemistry: three `O2` partner lines go into the per-molecule bulk-reaction list, which is today's acid-base list renamed. A kernel-free `ScavengerSpec` module does the parsing, unit conversion and checks, and a unit test covers it.

**Tech Stack:** C++20, Geant4 11.4.1 (Geant4-DNA chemistry: `G4DNAScavengerProcess`, `G4DNAScavengerMaterial`, `G4VChemistryWorld`), CMake + Ninja, MSVC x64, plain-`assert` + CTest.

**Spec:** `.work/sessions/2026-09-28__o2-scavenger-uhdr/01-grill/resume.md`, decision record `docs/adr/0004-scavenger-reactions-per-chemistry.md`, vocabulary `CONTEXT.md` (Scavenger, Bulk species, Bulk reaction).

## Global Constraints

- SBS is the only chemistry time-step model. Add no UHDR pulse structure, no dose-rate features, no voxelization.
- Diagnostics go through `DnaLogger` (`core/DnaLogger.hh`). No ad hoc `G4cout`.
- Unit tests are pure logic only (no Geant4 kernel). Build and run them from `build-ninja/` (Debug); `build/` defines `NDEBUG`.
- Project includes are rooted at `header/` (`#include "geometry/ScavengerSpec.hh"`).
- UHDR rates: `e_aq + O2(B) -> O2m` 1.74e10, `H + O2(B) -> HO2°` 2.1e10, `Om + O2(B) -> O3m` 3.7e9 M^-1 s^-1, reactionType unset (0).
- `%` unit is O2 only, kH = 0.0013 M (100 % = pure-O2 atmosphere).
- Smoke runs: 10 keV e-, `/run/beamOn 2`, scratch macro in `build/macro/` (untracked), output isolated with `--dir`.
- Match Geant4 naming (PascalCase files/classes, camelCase variables, `f` member prefix) and the surrounding file style.

## Review Focus

1. **Bulk O2 absorbs O2 products.** Checked in `G4DNAScavengerProcess::PostStepDoIt` and `G4DNAMakeReaction`: while bulk O2 > 0, every `O2` product, from a tracked or a bulk reaction, goes into the bulk pool instead of becoming a track (UHDR does the same). With `O2 21 %`, tracked `O2` in `Species.Txt` drops to about zero, and the tracked `e_aq/H/Om + O2` table reactions have nothing to react with. Ticket 04 records the observed G(O2) and documents this in ADR 0004 and CLAUDE.md.
2. **Wrong case (`o2 21 %`).** Species names are case-sensitive. Expect a fatal error whose message names `O2`, not a silent inert scavenger. Test in ticket 02.
3. **Setting a species back to 0 after a non-zero value** removes it (last wins, 0 = absent). It must not appear in the bulk composition or in the inert warning. Tests in ticket 02 (Upsert + InertSpecies) and ticket 03 (composition log).
4. **`/chem/env/scavenger` issued after `/run/initialize`.** Geant4 refuses it (PreInit only) and the batch macro stops. It must not be silently applied too late. Scratch-macro check in ticket 03.
5. **MT run (`--threads 2`) with `O2 21 %`.** The command is master-only (`SetToBeBroadcasted(false)`). Each worker's `G4DNAScavengerMaterial` reads the composition the master built. Expect no fatal error, and the dump lists the O2(B) lines. Smoke check in ticket 04.

---

## Shared procedures (referenced by every ticket)

**MSVC wrapper** (PowerShell tool; each build command runs inside it):

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build --config RelWithDebInfo --target sim"
```

**Unit tests** (same wrapper; build every test target, then run):

```powershell
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure"
```

Pass condition: every test `Passed`, none `Not Run`.

**Smoke macro** `build/macro/scav_smoke.in` (scratch, untracked; `<SCAVENGER_LINE>` is empty or one `/chem/env/scavenger` line):

```text
/run/verbose 0
/dnaLogger/verbose Warning
/process/dna/e-SolvationSubType Ritchie1994
<SCAVENGER_LINE>
/chem/reaction/dump reactions_dump.txt
/run/initialize
/gun/particle e-
/gun/energy 10 keV
/run/beamOn 2
/run/dumpDataAndReset
```

Run from `build/` in the background: `./sim scav_smoke.in --dir smoke/<name> > smoke/<name>.log 2>&1` (create `smoke/` first; `--dir` needs the parent to exist). Success markers: `[RunAccumulatorMessenger] dumped and reset` and `The simulation took`. Failure markers: `EEEE`, `FatalException`. File formats: `.claude/skills/sim-output/SKILL.md`.

**Deterministic comparison.** Species yields are not reproducible run to run (chemistry stepping diverges). Compare only (a) `reactions_dump.txt` with lines sorted (`sort`; the bulk map is keyed by pointer, so line order varies) and (b) `EnergyDeposit.Txt` + `PhysicsInteractions.csv`, which are byte-identical for the same macro (fixed seed `kDefaultSeed` in `sim.cc`).

---

### Task 1: Rename acid-base list to bulk-reaction list (no behavior change)

**Files:**
- Modify: `header/chemistry/ChemistryTypes.hh`
- Modify: `header/chemistry/ChemistryRegistry.hh`, `src/chemistry/ChemistryRegistry.cc`
- Modify: `src/chemistry/BuiltInChemistries.cc`
- Modify: `header/chemistry/catalog/PureWaterReactions.hh`, `src/chemistry/catalog/PureWaterReactions.cc`
- Modify: `header/chemistry/catalog/BoscoloChemReactions.hh`, `src/chemistry/catalog/BoscoloChemReactions.cc`
- Modify: `header/chemistry/DnaChemistryList.hh`, `src/chemistry/DnaChemistryList.cc`
- Modify: `header/chemistry/ReactionTableDump.hh`, `src/chemistry/ReactionTableDump.cc`
- Test: `test/ChemistryRegistryTest.cc`
- Commit alongside: `docs/adr/0004-scavenger-reactions-per-chemistry.md` (new), `docs/adr/0001-baseline-acid-base-buffer.md`, `CONTEXT.md` (grill output, already edited)

**Interfaces:**
- Produces:
  - `ChemistryTypes::BulkReaction { std::string partner; double rate; std::vector<std::string> products; int reactionType = 0; }`
  - `ChemistryTypes::BulkReactionEntry { std::string molecule; std::vector<BulkReaction> reactions; }`
  - `using ChemistryTypes::BulkReactionList = std::vector<BulkReactionEntry>;`
  - `ChemistryRegistry::Chemistry::buildBulkReactions` of type `ChemistryTypes::BulkReactionList (*)()`
  - `PureWaterReactions::BuildPureWaterBulkReactions()`, `BoscoloChemReactions::BuildBoscoloChemBulkReactions()`
  - `DnaChemistryList::RegisterBulkReactionProcesses(const G4DNABoundingBox&, const ChemistryTypes::BulkReactionList&) const`
  - `ReactionTableDump::WriteBulkReactions(std::ostream&)`; dump section header `# Bulk reactions (acid-base buffer + scavengers)`

- [ ] **Step 1: Capture the baseline before any edit.** Build `sim`, write the smoke macro with an empty `<SCAVENGER_LINE>`, run it with `--dir smoke/baseline`. Save `sort smoke/baseline/reactions_dump.txt > smoke/baseline.sorted`. Tickets 01, 03 and 04 compare against this baseline.

- [ ] **Step 2: Rename the types in `ChemistryTypes.hh`.**

```cpp
  /// One first-order or pseudo-first-order reaction of a tracked molecule
  /// against a bulk species: an acid-base buffer partner ("H3Op(B)",
  /// "OHm(B)", "H2O") or a scavenger ("O2"). rate is already in Geant4
  /// internal units. reactionType 0 = leave unset.
  struct BulkReaction
  {
    std::string partner;
    double rate;
    std::vector<std::string> products;
    int reactionType = 0;
  };

  /// All bulk reactions registered as one G4DNAScavengerProcess on `molecule`
  /// (Geant4 allows one such process per molecule).
  struct BulkReactionEntry
  {
    std::string molecule;
    std::vector<BulkReaction> reactions;
  };

  /// May be empty: a Chemistry without bulk reactions.
  using BulkReactionList = std::vector<BulkReactionEntry>;
```

- [ ] **Step 3: Update `test/ChemistryRegistryTest.cc` first** (`DummyAcidBase` -> `DummyBulkReactions` returning `ChemistryTypes::BulkReactionList`). Build `ChemistryRegistryTest` in `build-ninja`. Expected: compile FAIL, because `ChemistryRegistry::Chemistry` still uses `AcidBaseList`.

- [ ] **Step 4: Rename everywhere else.** In `ChemistryRegistry.hh` the field becomes `ChemistryTypes::BulkReactionList (*buildBulkReactions)();`; update `ChemistryRegistry.cc` if it names the field (null-builder check). Rename the builders (declarations, definitions, `BuiltInChemistries.cc` registrations), `RegisterAcidBaseScavengerProcesses` -> `RegisterBulkReactionProcesses` (including its `caller` string), and `WriteAcidBase` -> `WriteBulkReactions`. Change the dump section header in `DumpReactionTable` to `"\n# Bulk reactions (acid-base buffer + scavengers)\n"` and the `/chem/reaction/dump` guidance to "(bimolecular + bulk networks)". Reword the doc comments that call the list "acid-base" into "bulk reactions (acid-base buffer ...)". The rates and entries in both builders stay exactly the same.

- [ ] **Step 5: Check nothing is left.** Run `grep -rn "AcidBase\|acid-base list" header src test`. Expected: no hits. "acid-base buffer" as the name of the chemistry itself may stay.

- [ ] **Step 6: Build and test.** Build `sim` (RelWithDebInfo), then run the unit-test procedure. Expected: all tests Passed.

- [ ] **Step 7: Check that behavior is unchanged.** Rerun the smoke macro with `--dir smoke/t01`. `diff <(sort smoke/t01/reactions_dump.txt) smoke/baseline.sorted` must differ only in the section-header line. `EnergyDeposit.Txt` and `PhysicsInteractions.csv` must be identical to `smoke/baseline/`.

- [ ] **Step 8: Commit.**

```bash
git add header/chemistry src/chemistry test/ChemistryRegistryTest.cc docs/adr/0004-scavenger-reactions-per-chemistry.md docs/adr/0001-baseline-acid-base-buffer.md CONTEXT.md
git commit -m "refactor: rename acid-base list to bulk-reaction list (ADR 0004)"
```

---

### Task 2: `ScavengerSpec` parsing and checks, with unit test

**Files:**
- Create: `header/geometry/ScavengerSpec.hh`, `src/geometry/ScavengerSpec.cc`
- Create: `test/ScavengerSpecTest.cc`
- Modify: `CMakeLists.txt` (add the test target after `ChemistryRegistryTest`)

**Interfaces:**
- Consumes: `ChemistryTypes::BulkReactionList` (Task 1).
- Produces (namespace `ScavengerSpec`):
  - `constexpr double kO2SaturationMolarity = 0.0013;`
  - `struct Entry { std::string species; double molarity = 0.; };` (molarity in mol/L, plain double, no Geant4 units)
  - `using List = std::vector<Entry>;`
  - `bool Parse(const std::string& text, Entry& out, std::string& err);`
  - `bool IsPhOwned(const std::string& species);`
  - `void Upsert(List& list, const Entry& entry);`
  - `std::vector<std::string> InertSpecies(const List& list, const ChemistryTypes::BulkReactionList& reactions);`

Deviation from the resume: pH-owned names are rejected in `Parse` (PreInit), not later in `ConstructChemistryComponents`. The check needs only the name, so it can fail earlier. Unknown species still fail at `/run/initialize` (Task 3), because the molecule table is empty at PreInit.

- [ ] **Step 1: Write the failing test** `test/ScavengerSpecTest.cc`:

```cpp
/// \file ScavengerSpecTest.cc
/// \brief Plain-assert unit tests for ScavengerSpec (no Geant4 runtime).
#include "geometry/ScavengerSpec.hh"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
bool Near(double a, double b) { return std::fabs(a - b) <= 1e-12 * std::fabs(b) + 1e-300; }

bool Contains(const std::string& text, const std::string& part)
{
  return text.find(part) != std::string::npos;
}

ScavengerSpec::Entry MustParse(const std::string& text)
{
  ScavengerSpec::Entry entry;
  std::string err;
  const bool ok = ScavengerSpec::Parse(text, entry, err);
  if (!ok) std::cerr << "unexpected parse failure: " << err << "\n";
  assert(ok);
  (void)ok;
  return entry;
}

std::string MustFail(const std::string& text)
{
  ScavengerSpec::Entry entry;
  std::string err;
  const bool ok = ScavengerSpec::Parse(text, entry, err);
  assert(!ok);
  assert(!err.empty());
  (void)ok;
  return err;
}
}  // namespace

static void TestUnitsConvertToMolarity()
{
  assert(Near(MustParse("O2 21 %").molarity, 0.21 * 0.0013));
  assert(Near(MustParse("O2 100 %").molarity, 0.0013));
  assert(Near(MustParse("H2O2 1 mM").molarity, 1e-3));
  assert(Near(MustParse("H2O2 2 uM").molarity, 2e-6));
  assert(Near(MustParse("H2O2 0.5 M").molarity, 0.5));
  assert(MustParse("O2 21 %").species == "O2");
}

static void TestZeroIsAccepted()
{
  assert(MustParse("O2 0 %").molarity == 0.);
}

static void TestRejectsBadValuesAndUnits()
{
  assert(Contains(MustFail("O2 -1 %"), "negative"));
  assert(Contains(MustFail("O2 21 mol"), "unknown unit"));
  assert(Contains(MustFail("O2 abc %"), "invalid concentration"));
  assert(Contains(MustFail("O2 1x %"), "invalid concentration"));
  assert(Contains(MustFail("O2 nan M"), "invalid concentration"));
  MustFail("O2 21");
  MustFail("O2 21 % extra");
  MustFail("");
}

static void TestPercentIsO2Only()
{
  assert(Contains(MustFail("CO2 1 %"), "only defined for O2"));
  // Wrong case: names are case-sensitive, so "o2" is not O2.
  assert(Contains(MustFail("o2 21 %"), "O2"));
}

static void TestPhOwnedSpeciesRejected()
{
  for (const char* name : {"H2O", "H2O(B)", "H3Op(B)", "OHm(B)"}) {
    assert(ScavengerSpec::IsPhOwned(name));
    assert(Contains(MustFail(std::string(name) + " 1 M"), "/chem/env/pH"));
  }
  assert(!ScavengerSpec::IsPhOwned("O2"));
  assert(!ScavengerSpec::IsPhOwned("H3Op"));  // tracked H3O+, not the bulk pool
}

static void TestUpsertLastWins()
{
  ScavengerSpec::List list;
  ScavengerSpec::Upsert(list, {"O2", 1.0});
  ScavengerSpec::Upsert(list, {"H2O2", 2.0});
  ScavengerSpec::Upsert(list, {"O2", 3.0});
  assert(list.size() == 2);
  assert(list[0].species == "O2" && list[0].molarity == 3.0);
  assert(list[1].species == "H2O2" && list[1].molarity == 2.0);
}

static void TestInertSpecies()
{
  const ChemistryTypes::BulkReactionList reactions = {
    {"e_aq", {{"O2", 1.0, {"O2m"}, 0}, {"H3Op(B)", 1.0, {"H"}, 0}}},
  };
  ScavengerSpec::List list;
  ScavengerSpec::Upsert(list, {"O2", 2.7e-4});
  ScavengerSpec::Upsert(list, {"H2O2", 1e-3});
  ScavengerSpec::Upsert(list, {"NO3m", 1e-3});
  ScavengerSpec::Upsert(list, {"NO3m", 0.});  // set back to 0 -> absent, not inert
  const auto inert = ScavengerSpec::InertSpecies(list, reactions);
  assert(inert.size() == 1);
  assert(inert[0] == "H2O2");
  assert(ScavengerSpec::InertSpecies(list, {}).size() == 2);  // empty Chemistry list
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestUnitsConvertToMolarity();
  TestZeroIsAccepted();
  TestRejectsBadValuesAndUnits();
  TestPercentIsO2Only();
  TestPhOwnedSpeciesRejected();
  TestUpsertLastWins();
  TestInertSpecies();
  std::cout << "ScavengerSpecTest: all tests passed\n";
  return 0;
}
```

Add to `CMakeLists.txt` after the `ChemistryRegistryTest` block:

```cmake
# Standalone unit test for ScavengerSpec (parsing /chem/env/scavenger values,
# unit conversion and checks -- pure logic, no Geant4 kernel dependency).
add_executable(ScavengerSpecTest test/ScavengerSpecTest.cc src/geometry/ScavengerSpec.cc)
target_link_libraries(ScavengerSpecTest ${Geant4_LIBRARIES})
target_include_directories(ScavengerSpecTest PRIVATE ${project_include_dirs})
add_test(NAME ScavengerSpecTest COMMAND ScavengerSpecTest)
```

- [ ] **Step 2: Run it and confirm it fails.** Build `ScavengerSpecTest` in `build-ninja`. Expected: FAIL (missing `geometry/ScavengerSpec.hh`).

- [ ] **Step 3: Write the header** `header/geometry/ScavengerSpec.hh`:

```cpp
/// \file ScavengerSpec.hh
/// \brief /chem/env/scavenger values: parsing, conversion to molarity, and the
/// checks that need no Geant4 kernel.
///
/// Pure logic (standard library + ChemistryTypes.hh): no Geant4 kernel, no
/// logging. Errors are returned as bool + message; the caller raises the
/// G4Exception. Molarities are plain mol/L doubles; DnaChemistryWorld applies
/// Geant4 units. Unit rules follow the UHDR example's ChemistryWorld.
#ifndef ScavengerSpec_h
#define ScavengerSpec_h 1

#include "chemistry/ChemistryTypes.hh"

#include <string>
#include <vector>

namespace ScavengerSpec
{
  /// O2 molarity in water under a pure-O2 atmosphere (Henry's law, UHDR), in M.
  constexpr double kO2SaturationMolarity = 0.0013;

  /// One exogenous bulk scavenger: species as named in G4MoleculeTable,
  /// concentration in mol/L (0 = absent).
  struct Entry
  {
    std::string species;
    double molarity = 0.;
  };

  using List = std::vector<Entry>;

  /// Parses "<species> <value> <unit>". unit: M, mM, uM, or % (O2 only, % of
  /// kO2SaturationMolarity). False + err for a wrong token count, a
  /// non-numeric or non-finite value, a negative value, an unknown unit, % on
  /// a species other than O2, or a pH-owned species (IsPhOwned).
  bool Parse(const std::string& text, Entry& out, std::string& err);

  /// True for the bulk species set by the water/pH model: H2O, H2O(B),
  /// H3Op(B), OHm(B).
  bool IsPhOwned(const std::string& species);

  /// Replaces the entry with the same species (last wins) or appends it.
  void Upsert(List& list, const Entry& entry);

  /// Species with molarity > 0 that are not the partner of any reaction in
  /// `reactions`, in list order: they would sit in the bulk without reacting.
  std::vector<std::string> InertSpecies(const List& list,
                                        const ChemistryTypes::BulkReactionList& reactions);
}  // namespace ScavengerSpec

#endif  // ScavengerSpec_h
```

- [ ] **Step 4: Write the implementation** `src/geometry/ScavengerSpec.cc`:

```cpp
/// \file ScavengerSpec.cc
/// \brief Implementation of the ScavengerSpec parsing and checks
#include "geometry/ScavengerSpec.hh"

#include <cmath>
#include <exception>
#include <set>
#include <sstream>

bool ScavengerSpec::Parse(const std::string& text, Entry& out, std::string& err)
{
  std::istringstream iss(text);
  std::string species;
  std::string valueText;
  std::string unit;
  std::string extra;
  if (!(iss >> species >> valueText >> unit) || (iss >> extra)) {
    err = "expected '<species> <value> <unit>', got '" + text + "'";
    return false;
  }

  if (IsPhOwned(species)) {
    err = species + " is set by the water/pH model (/chem/env/pH), not by /chem/env/scavenger";
    return false;
  }

  double value = 0.;
  std::size_t used = 0;
  try {
    value = std::stod(valueText, &used);
  }
  catch (const std::exception&) {
    used = 0;
  }
  if (used == 0 || used != valueText.size() || !std::isfinite(value)) {
    err = "invalid concentration '" + valueText + "' for " + species;
    return false;
  }
  if (value < 0.) {
    err = "negative concentration " + valueText + " for " + species;
    return false;
  }

  double factor = 0.;
  if (unit == "M") {
    factor = 1.;
  }
  else if (unit == "mM") {
    factor = 1e-3;
  }
  else if (unit == "uM") {
    factor = 1e-6;
  }
  else if (unit == "%") {
    if (species != "O2") {
      err = "unit % is only defined for O2 (Henry's law, kH = 0.0013 M), not for " + species;
      return false;
    }
    factor = kO2SaturationMolarity / 100.;
  }
  else {
    err = "unknown unit '" + unit + "' for " + species + " (use M, mM, uM, or % for O2)";
    return false;
  }

  out = {species, value * factor};
  return true;
}

bool ScavengerSpec::IsPhOwned(const std::string& species)
{
  return species == "H2O" || species == "H2O(B)" || species == "H3Op(B)" || species == "OHm(B)";
}

void ScavengerSpec::Upsert(List& list, const Entry& entry)
{
  for (auto& existing : list) {
    if (existing.species == entry.species) {
      existing.molarity = entry.molarity;
      return;
    }
  }
  list.push_back(entry);
}

std::vector<std::string> ScavengerSpec::InertSpecies(
  const List& list, const ChemistryTypes::BulkReactionList& reactions)
{
  std::set<std::string> partners;
  for (const auto& entry : reactions) {
    for (const auto& reaction : entry.reactions) {
      partners.insert(reaction.partner);
    }
  }

  std::vector<std::string> inert;
  for (const auto& scavenger : list) {
    if (scavenger.molarity > 0. && partners.count(scavenger.species) == 0) {
      inert.push_back(scavenger.species);
    }
  }
  return inert;
}
```

- [ ] **Step 5: Run the tests and confirm they pass.** Run the unit-test procedure. Expected: `ScavengerSpecTest` Passed, all others Passed.

- [ ] **Step 6: Commit.**

```bash
git add header/geometry/ScavengerSpec.hh src/geometry/ScavengerSpec.cc test/ScavengerSpecTest.cc CMakeLists.txt
git commit -m "feat: add ScavengerSpec parser for /chem/env/scavenger values"
```

---

### Task 3: `/chem/env/scavenger` command, bulk composition and validation

**Files:**
- Create: `header/geometry/ScavengerMessenger.hh`, `src/geometry/ScavengerMessenger.cc`
- Modify: `header/geometry/DnaChemistryWorld.hh`, `src/geometry/DnaChemistryWorld.cc`
- Modify: `src/chemistry/DnaChemistryList.cc` (`ConstructReactionTable`: inert warning)
- Modify: `macro/beam_o2.in`, `macro/beam.in`
- Modify: `CLAUDE.md` (Macro and Logging paragraph; `DnaChemistryList.cc` Key Files line on `/chem/env/O2`)

**Interfaces:**
- Consumes: `ScavengerSpec::Parse/Upsert/InertSpecies/Entry/List` (Task 2); `ChemistryRegistry::Chemistry::buildBulkReactions` (Task 1).
- Produces:
  - `void DnaChemistryWorld::SetScavenger(const ScavengerSpec::Entry& entry);` (last wins)
  - `const ScavengerSpec::List& DnaChemistryWorld::GetScavengers() const;`
  - Removed: `SetOxygenPercent`, `GetOxygenConcentration`, `IsOxygenScavengerEnabled`, `fO2Percent`, the `/chem/env/O2` command.
  - `class ScavengerMessenger : public G4UImessenger` with `explicit ScavengerMessenger(DnaChemistryWorld* world)`.

No unit test here: this is Geant4-object code, so the scratch macros below check it.

- [ ] **Step 1: Write the messenger header** `header/geometry/ScavengerMessenger.hh`:

```cpp
/// \file ScavengerMessenger.hh
/// \brief /chem/env/scavenger <species> <value> <unit> (PreInit only).
///
/// A plain G4UImessenger: the command takes three tokens, which
/// G4GenericMessenger's method dispatch cannot pass through (it cuts a
/// string argument to its first token). Attaches to the /chem/env/ directory
/// that DnaChemistryWorld's G4GenericMessenger creates, so construct it after
/// that one. Parsing and checks: ScavengerSpec.
#ifndef ScavengerMessenger_h
#define ScavengerMessenger_h 1

#include "G4UImessenger.hh"

class DnaChemistryWorld;
class G4UIcommand;

class ScavengerMessenger : public G4UImessenger
{
public:
  explicit ScavengerMessenger(DnaChemistryWorld* world);
  ~ScavengerMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
  DnaChemistryWorld* fpWorld;
  G4UIcommand* fpScavengerCmd;
};

#endif  // ScavengerMessenger_h
```

- [ ] **Step 2: Write the messenger** `src/geometry/ScavengerMessenger.cc`:

```cpp
/// \file ScavengerMessenger.cc
#include "geometry/ScavengerMessenger.hh"

#include "geometry/DnaChemistryWorld.hh"
#include "geometry/ScavengerSpec.hh"

#include "G4ApplicationState.hh"
#include "G4UIcommand.hh"
#include "G4UIparameter.hh"

#include <string>

ScavengerMessenger::ScavengerMessenger(DnaChemistryWorld* world) : fpWorld(world)
{
  fpScavengerCmd = new G4UIcommand("/chem/env/scavenger", this);
  fpScavengerCmd->SetGuidance(
    "Set an exogenous dissolved species as a bulk scavenger: <species> <value> <unit>.");
  fpScavengerCmd->SetGuidance(
    "unit: M, mM, uM, or % (O2 only: % of a pure-O2 atmosphere, kH = 0.0013 M).");
  fpScavengerCmd->SetGuidance(
    "Repeating a species replaces its value; 0 means absent. The reactions come from the "
    "selected Chemistry (bulk reactions). Must be issued before /run/initialize.");
  // All three are strings so ScavengerSpec reports every malformed value as a
  // fatal error, instead of the UI rejecting a bad number non-fatally.
  fpScavengerCmd->SetParameter(new G4UIparameter("species", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("value", 's', false));
  fpScavengerCmd->SetParameter(new G4UIparameter("unit", 's', false));
  fpScavengerCmd->AvailableForStates(G4State_PreInit);
  // The world lives on the master only (as in the UHDR example).
  fpScavengerCmd->SetToBeBroadcasted(false);
}

ScavengerMessenger::~ScavengerMessenger()
{
  delete fpScavengerCmd;
}

void ScavengerMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command != fpScavengerCmd) {
    return;
  }
  ScavengerSpec::Entry entry;
  std::string err;
  if (!ScavengerSpec::Parse(newValue, entry, err)) {
    G4Exception("ScavengerMessenger::SetNewValue", "InvalidScavenger", FatalException,
                err.c_str());
    return;
  }
  fpWorld->SetScavenger(entry);
}
```

- [ ] **Step 3: Update `DnaChemistryWorld.hh`.** Rewrite the header comment UI block:

```cpp
/// UI (all G4State_PreInit, directory /chem/env/):
///   /chem/env/pH <double>                        bulk water pH (default 7)
///   /chem/env/scavenger <species> <value> <unit> exogenous bulk scavenger;
///       unit M, mM, uM, or % (O2 only, kH = 0.0013 M); repeat = last wins;
///       0 = absent (ScavengerMessenger, ScavengerSpec)
/// A scavenger's reactions are bulk reactions of the selected Chemistry
/// (docs/adr/0004-scavenger-reactions-per-chemistry.md).
```

Replace the O2 accessors and `fO2Percent` with:

```cpp
  /// Adds or replaces (last wins) one exogenous bulk scavenger.
  void SetScavenger(const ScavengerSpec::Entry& entry) { ScavengerSpec::Upsert(fScavengers, entry); }
  const ScavengerSpec::List& GetScavengers() const { return fScavengers; }

private:
  std::unique_ptr<G4GenericMessenger> fMessenger;
  std::unique_ptr<ScavengerMessenger> fScavengerMessenger;
  G4double fpH = 7.0;
  G4double fHalfBox = 500. * um;
  ScavengerSpec::List fScavengers;
```

Add `#include "geometry/ScavengerSpec.hh"` and the forward declaration `class ScavengerMessenger;`. Also update the class comment at the top ("optionally dissolved O2" becomes "optional exogenous scavengers").

- [ ] **Step 4: Update `DnaChemistryWorld.cc`.** In the constructor, delete the `o2Cmd` block and add `fScavengerMessenger = std::make_unique<ScavengerMessenger>(this);` after the pH command (the `/chem/env/` directory exists by then). Include `geometry/ScavengerMessenger.hh`. Because the destructor is defaulted in the `.cc`, `unique_ptr` to the incomplete type is fine. In `ConstructChemistryComponents`, replace the O2 block and the log line:

```cpp
  G4String scavengerSummary;
  for (const auto& scavenger : fScavengers) {
    if (scavenger.molarity <= 0.) {
      continue;  // 0 = absent
    }
    auto* conf = table->GetConfiguration(scavenger.species, false);
    if (conf == nullptr) {
      G4Exception("DnaChemistryWorld::ConstructChemistryComponents", "UnknownScavenger",
                  FatalException,
                  ("/chem/env/scavenger: species \"" + scavenger.species +
                   "\" is not in the molecule table.")
                    .c_str());
      return;
    }
    fpChemicalComponent[conf] = scavenger.molarity / (mole * liter);
    scavengerSummary += ", " + scavenger.species + " = " + std::to_string(scavenger.molarity) + " M";
  }

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryWorld] bulk composition: pH = " + std::to_string(fpH) +
                     scavengerSummary + " (" + std::to_string(fpChemicalComponent.size()) +
                     " components)");
```

- [ ] **Step 5: Add the inert warning in `DnaChemistryList::ConstructReactionTable`.** Look up the Chemistry before building, then warn after the world composition is built. `ConstructReactionTable` runs once on the master, so the warning prints once:

```cpp
  const auto* chemistry = SelectedChemistry("ConstructReactionTable");
  for (const auto& species :
       ScavengerSpec::InertSpecies(chemWorld->GetScavengers(), chemistry->buildBulkReactions())) {
    DnaLogger::Print(DnaLogger::Level::Warning,
                     "[DnaChemistryList] scavenger " + species +
                       " is inert: no bulk reaction of chemistry " + chemistry->name +
                       " uses it as a partner");
  }
  chemistry->buildReactions(reactionTable);
```

Include `geometry/ScavengerSpec.hh`. Update the class doc comment in `DnaChemistryList.hh` and the file comment in `DnaChemistryList.cc`: remove "`/chem/env/O2` currently has no effect here" and "whether O2 is enabled", and say the scavenger reactions are bulk reactions of the Chemistry.

- [ ] **Step 6: Migrate the macros.** In `macro/beam_o2.in`, replace lines 8-12 with:

```text
# Dissolved O2 as a bulk scavenger: 21 % of a pure-O2 atmosphere
# (air-equilibrated water, kH = 0.0013 M -> 2.73e-4 M). The reactions
# (e_aq/H/O- + O2 bulk) come from the selected Chemistry.
/chem/env/scavenger O2 21 %
```

In `macro/beam.in`, replace lines 11-13 with:

```text
# Optional dissolved-O2 bulk scavenger (0 or absent = anoxic).
# Air-equilibrated water ~= 21 % of a pure-O2 atmosphere.
#/chem/env/scavenger O2 21 %
```

Also run `grep -rn "env/O2" macro` and migrate any other hit the same way.

- [ ] **Step 7: Update CLAUDE.md.** In "Macro and Logging", replace the `/chem/env/O2 <percent>` sentence with: `/chem/env/scavenger <species> <value> <unit>` (PreInit; `M`/`mM`/`uM`, or `%` for O2 only with kH = 0.0013 M; repeat = last wins; 0 = absent; unknown unit, negative value, `%` on another species or a pH-owned species (`H2O`, `H3Op(B)`, `OHm(B)`) is fatal at the command; a species not in the molecule table is fatal at `/run/initialize`; a species no bulk reaction of the selected Chemistry uses triggers a `DnaLogger` warning). Also change the `beam_o2.in` example to "(sets `/chem/env/scavenger O2 21 %`; pH = 7)". In the `DnaChemistryList.cc` Key Files line, replace "Only an exogenous dissolved-O2 supply mechanism remains deferred to future work — `/chem/env/O2` currently has no effect on the chemistry." with "Exogenous scavengers (`/chem/env/scavenger`, `DnaChemistryWorld`) react through the Chemistry's bulk reactions (ADR 0004)." and rename "acid-base list" to "bulk-reaction list" in that line.

- [ ] **Step 8: Build and run the regression check.** Build `sim` and the unit tests (all Passed). Run the smoke macro with an empty `<SCAVENGER_LINE>`, `--dir smoke/t03`. The sorted dump must equal `smoke/t01`'s, and the physics files must be identical to `smoke/baseline/`.

- [ ] **Step 9: Check the command behavior with scratch macros** (each is `<line>` then `/run/initialize`, no beamOn; run from `build/`, read the log):

| `<line>` | Expected |
|---|---|
| `/chem/env/scavenger O2 21 mol` | `FatalException` `InvalidScavenger`, message "unknown unit" |
| `/chem/env/scavenger CO2 1 %` | fatal, "only defined for O2" |
| `/chem/env/scavenger O2 -1 %` | fatal, "negative" |
| `/chem/env/scavenger H3Op(B) 1 M` | fatal, "/chem/env/pH" |
| `/chem/env/scavenger Foo 1 mM` | fatal at init, `UnknownScavenger` |
| `/dnaLogger/verbose Info` + `/chem/env/scavenger H2O2 1 mM` | init succeeds; log has `scavenger H2O2 is inert` and the composition lists `H2O2 = 0.001000 M` |
| `/dnaLogger/verbose Info` + `/chem/env/scavenger O2 21 %` + `/chem/env/scavenger O2 0 %` | composition log has no O2 entry, no inert warning |
| `/run/initialize` first, then `/chem/env/scavenger O2 21 %` | Geant4 refuses the command (illegal application state), the batch stops; no silent late apply |
| `/chem/env/O2 21` | command not found (command removed) |

- [ ] **Step 10: Commit.**

```bash
git add header/geometry src/geometry src/chemistry/DnaChemistryList.cc header/chemistry/DnaChemistryList.hh macro/beam_o2.in macro/beam.in CLAUDE.md
git commit -m "feat: replace /chem/env/O2 with generic /chem/env/scavenger bulk command"
```

---

### Task 4: O2(B) bulk reactions in PureWater and BoscoloChem, with acceptance runs

**Files:**
- Modify: `src/chemistry/catalog/PureWaterReactions.cc`, `header/chemistry/catalog/PureWaterReactions.hh`
- Modify: `src/chemistry/catalog/BoscoloChemReactions.cc`, `header/chemistry/catalog/BoscoloChemReactions.hh`
- Modify: `docs/adr/0004-scavenger-reactions-per-chemistry.md` (Consequences: O2 products absorbed)
- Modify: `CLAUDE.md` (`PureWaterReactions.cc` Key Files line)

**Interfaces:**
- Consumes: `ChemistryTypes::BulkReaction` (Task 1); `/chem/env/scavenger` (Task 3).
- Produces: no new symbols. The `e_aq`, `H` and `Om` entries of both bulk-reaction lists each gain one `"O2"` partner reaction.

- [ ] **Step 1: Write the failing check.** Run the smoke macro with `<SCAVENGER_LINE>` = `/chem/env/scavenger O2 21 %`, `--dir smoke/t04_pre`. Expected: the log warns `scavenger O2 is inert`, and `grep " + O2 ->" smoke/t04_pre/reactions_dump.txt` finds only the three bimolecular (tracked) lines, nothing in the bulk section.

- [ ] **Step 2: Add the reactions in `PureWaterReactions::BuildPureWaterBulkReactions`.** Change the `H`, `e_aq` and `Om` entries to:

```cpp
    {"H",
     {{"H2O", 6.32 / s, {"e_aq", "H3Op(B)"}, 0},
      {"OHm(B)", 2.49e7 * M, {"e_aq"}, 0},
      {"O2", 2.1e10 * M, {kHO2}, 0}}},
    {"e_aq",
     {{"H3Op(B)", 2.25e10 * M, {"H"}, 0},
      {"H2O", 1.57e1 * cW / s, {"H", "OHm(B)"}, 0},
      {"O2", 1.74e10 * M, {"O2m"}, 0}}},
```

```cpp
    {"Om",
     {{"H3Op(B)", 9.56e10 * M, {kOH}, 0},
      {"H2O", 1.8e6 * cW / s, {kOH, "OHm(B)"}, 8},
      {"O2", 3.7e9 * M, {"O3m"}, 0}}},
```

Above the return, add the comment: `// "O2" partner = the dissolved-O2 scavenger (bulk component set by /chem/env/scavenger; UHDR: EmDNAChemistry scavenger processes). Inert when its concentration is 0.` Replace the "Same values and order as the former hard-coded list" comment with this one. In `PureWaterReactions.hh`, rename the line "bulk-O2 scavenging: e_aq/H/O- + O2" to "e_aq/H/O- + O2 against *tracked* radiolytic O2". In the builder description, add "and the dissolved-O2 scavenger reactions e_aq/H/O- + O2 (bulk)".

- [ ] **Step 3: Apply the identical edit to `BoscoloChemReactions::BuildBoscoloChemBulkReactions`** and its header comment. The code is the same as Step 2, since BoscoloChem is a verbatim copy until someone edits it. Do not run BoscoloChem in any smoke test (user decision); a successful build is enough.

- [ ] **Step 4: Build and run the unit tests.** All Passed.

- [ ] **Step 5: Run the acceptance checks** (smoke macro, run in the background, read each log):
  - `smoke/t04_o2` with `/chem/env/scavenger O2 21 %`: no inert warning. The bulk section of `reactions_dump.txt` contains exactly `e_aq + O2 -> O2m    k = 1.74e+10`, `H + O2 -> HO2°    k = 2.1e+10`, `Om + O2 -> O3m    k = 3.7e+09` (the numbers print in stream default format).
  - `smoke/t04_zero` with an empty line: `diff <(sort smoke/t04_zero/reactions_dump.txt) <(sort smoke/t03/reactions_dump.txt)` shows exactly the three added O2 lines. `EnergyDeposit.Txt` and `PhysicsInteractions.csv` are identical to `smoke/baseline/`. Species G-values at 1 µs are plausible and close to `smoke/t03` (a 0 concentration keeps the reactions inert).
  - Direction check (2 events, not quantitative): compare `Species.Txt` of `t04_o2` with `t04_zero` at 1 µs. G(e_aq) and G(H) are clearly lower, G(O2m) is clearly higher. Record G(O2) in both runs: it should drop to about zero with bulk O2 (Review Focus 1).
  - MT: `./sim scav_smoke.in --threads 2 --dir smoke/t04_mt` with the O2 line. No `FatalException`, success markers present, and the dump lists the three O2(B) lines.
  - BoscoloChem is not run (user decision). Its edit in Step 3 is only checked by the build.

- [ ] **Step 6: Document the absorbed O2 products.** Append to ADR 0004's Consequences: "While a bulk O2 concentration is set, Geant4 sends every `O2` product (of tracked and bulk reactions alike) into the bulk pool instead of creating a track (`G4DNAScavengerProcess`, `G4DNAMakeReaction`), as in the UHDR example: tracked O2 then disappears from the species output, and the tracked `e_aq/H/O- + O2` reactions have nothing to react with." Add one sentence to the CLAUDE.md `PureWaterReactions.cc` Key Files line: the bulk list carries the three O2(B) scavenger reactions (UHDR rates), inert at 0 concentration, and with O2 > 0 radiolytic O2 is absorbed into the bulk.

- [ ] **Step 7: Clean up** `build/smoke/` and `build/macro/scav_smoke.in` (both scratch).

- [ ] **Step 8: Commit.**

```bash
git add src/chemistry/catalog header/chemistry/catalog docs/adr/0004-scavenger-reactions-per-chemistry.md CLAUDE.md
git commit -m "feat: add dissolved-O2 bulk scavenger reactions to PureWater and BoscoloChem"
```

---

## Sequencing rationale

Ticket 01 is a pure rename, checked against a baseline captured before the first edit. Doing it first means every later ticket uses the final names. Ticket 02 adds kernel-free code that nothing calls yet, so `sim` stays unchanged. Ticket 03 is the first user-visible change (command swap). It still leaves the chemistry unchanged at 0 %, which the regression check shows. Ticket 04 adds the reactions last, so the "inert" warning from ticket 03 serves as its failing check, and it runs the full acceptance matrix once.

## Risks and mitigations

- **Radiolytic O2 absorbed into the bulk when O2 > 0** (Review Focus 1). This is Geant4 behavior, the same as UHDR. Ticket 04 documents it rather than working around it.
- **Uncommitted, unrelated edits in `CLAUDE.md`** (the move to the `sim-output` skill) and the untracked `.claude/skills/`. Commit them separately before ticket 01, or tickets 03/04 will sweep them into their commits.
- **A 2-event direction check can be noisy.** If G(e_aq)/G(H) do not clearly drop, rerun with `/run/beamOn 10` before suspecting the wiring, and check first that the dump shows the O2(B) lines.
- **`G4UIcommand` joins the three parameters back into `newValue` as one space-separated string.** `ScavengerSpec::Parse` tokenizes it again. If Geant4 changes the separators, the ticket 03 scratch macros catch it.

## Assumptions

- The `sim` build in `build/` and the test build in `build-ninja/` are already configured (incremental builds only).
- The ADR 0004 text written during grill counts as accepted (the grill's open question). Ticket 01 commits it, and ticket 04 adds only the absorbed-O2 consequence.
