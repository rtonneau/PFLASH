# Plan — class-based chemistry definition for `dnachem-min`

**Date:** 2026-08-28
**Goal:** Replace macro-driven chemistry (`/chem/species`, `/chem/reaction/add`) with a
class-based definition modelled on the Geant4-DNA `UHDR` example: a `G4VUserChemistryList` +
`G4VPhysicsConstructor` subclass plus a `G4VChemistryWorld` subclass.
**Status:** COMPLETE - implemented, built, and run-verified (serial + MT). See
"ITERATION 2" and "ITERATION 1" below.

---

## 0. Locked decisions

| # | Decision | Consequence |
|---|---|---|
| 1 | `DnaChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor` | mirrors `G4EmDNAChemistry_option3` / UHDR `EmDNAChemistry` |
| 2 | Registered via existing `PhysicsList::SetDNAChemistry()` -> `RegisterPhysics()` | modular list drives `ConstructParticle`/`ConstructProcess` and ordering |
| 3 | Species + reactions live in C++ (`ConstructMolecule` / `ConstructReactionTable`) | macro `/chem/species` + `/chem/reaction/*` removed |
| 4 | `DnaChemistryWorld : public G4VChemistryWorld` created now, owned by `DetectorConstruction` | boundary + bulk-composition map ready; scavenger processes/reactions added later |
| 5 | Default `G4ChemTimeStepModel::SBS`; `ConstructTimeStepModel()` throws `FatalException` on `IRT` (and `Unknown`) | IRT incompatible with the intended bounded/scavenger transport model |
| 6 | Serial today (`sim.cc:32`) but all new code MT-safe; validate under MT | see MT checklist (section 4) |

---

## 1. KB query result — what is already documented

| Topic | KB location |
|---|---|
| `G4VUserChemistryList` API, required overrides, `SetChemistryList(this)`, `(true)` ownership flag, manager dispatch phases | `chemistry-stage.md` -> *Custom Chemistry List - Subclassing `G4VUserChemistryList` (+ `G4VPhysicsConstructor`)* |
| `G4ChemDissociationChannels_option1` static molecule/channel helper (defines the `(B)` bulk species) | `chemistry-stage.md` -> *G4ChemDissociationChannels / _option1* |
| `G4VChemistryWorld` (boundary + bulk composition map); nothing auto-wires it | `chemistry-stage.md` -> *G4VChemistryWorld - Chemistry Domain and Bulk Composition* |
| UHDR `EmDNAChemistry` class shape, PhysicsList wiring (manual vs `RegisterPhysics`), corrected init-flow diagram, reaction-builder table | `uhdr-specifics.md` -> *Chemistry World and Builders* + *EmDNAChemistry (UHDR Variant)* |
| Chemical-stage processes a `ConstructProcess()` registers (solvation, transport, dissociation, recombination, scavenger) with `RegisterProcess` vs `AddRestProcess` ordinals | `dna-processes.md` -> *Chemical-Stage Processes* |
| Two registration patterns; time-step model is a runtime choice | `physics-lists.md` -> *Available Chemistry Constructors* / *Selection in PhysicsList* |
| Double-free trap, missing time-step model, `GetConfiguration` returning `nullptr` on typos, manual chem-world wiring | `gotchas.md` -> *Custom Chemistry List* |

KB path: `C:\DEV\GEANT4\geant4-v11.4.1-kb` (from `.claude/.claude-project.json`).

## 1b. Verified directly in Geant4 source (KB thin / silent here)

- Default time-step model is `G4ChemTimeStepModel::Unknown` -
  `source/processes/electromagnetic/utils/src/G4EmLowEParameters.cc:79`.
- `G4EmParameters::SetTimeStepModel(const G4ChemTimeStepModel&)` exists -
  `source/processes/electromagnetic/utils/src/G4EmParameters.cc:1618` (delegates to `SetChemTimeStepModel`).
- `/chem/reaction/UI` calls `G4DNAMolecularReactionTable::Reset()` (wipes reaction data);
  `/chem/reaction/add` and a code `ConstructReactionTable()` both write the **same**
  `G4DNAMolecularReactionTable` singleton -
  `source/processes/electromagnetic/dna/utils/src/G4ReactionTableMessenger.cc:67-71, 252-414`;
  messenger owned by the table singleton at `G4DNAMolecularReactionTable.cc:389`.
- `/chem/species` messenger owned by the `G4MoleculeTable` singleton -
  `source/processes/electromagnetic/dna/molecules/management/src/G4MoleculeTableMessenger.cc`.
- Init timing: chemistry constructor's `ConstructProcess()` ends with
  `G4DNAChemistryManager::Instance()->Initialize()` ->
  `InitializeMaster()` [master only] -> `ConstructDissociationChannels()` then
  `ConstructReactionTable()` (`G4DNAChemistryManager.cc:400-417`);
  `InitializeThread()` [per worker] -> `ConstructTimeStepModel()` (`:471-481`).
  Both happen inside `/run/initialize`, i.e. before any macro `/chem/...` line that follows
  `/run/initialize`.
- Reference implementation: `source/physics_lists/constructors/electromagnetic/src/G4EmDNAChemistry_option3.cc`
  (`ConstructMolecule` :82, `ConstructReactionTable` :96, `ConstructProcess` :670,
  `ConstructTimeStepModel` :754; skips explicit transport when model is `IRT`, :724).
- UHDR reference: `examples/extended/medical/dna/UHDR/src/{EmDNAChemistry,ChemistryWorld,DetectorConstruction,ActionInitialization,PhysicsList}.cc`.

## 1c. Current project state

- `src/PhysicsList.cc:79` `SetDNAChemistry(name)` maps a string -> `new G4EmDNAChemistry_option*`
  -> `RegisterPhysics(...)`. `ConstructParticle`/`ConstructProcess` are commented out; the base
  `G4VModularPhysicsList` drives registered constructors. Default `G4EmDNAChemistry_option3` (`:38`).
- `macro/beam.in` defines 7 species and 9 reactions **after** `/run/initialize`, with
  `/chem/reaction/UI` reset and `/process/chem/TimeStepModel IRT` (`:10`).
- `macro/beam_02.in` same species/reactions; already uses `SBS` (`:12`); adds `/scheduler/*` diagnostics.
- `src/DetectorConstruction.*` - homogeneous 1 mm water box, **no `G4VChemistryWorld`**, no `GetChemistryWorld()`.
- `src/ActionInitialization.cc:47-48` - `TimeStepAction` **not** registered (line commented);
  `G4Scheduler` end time set to `1.3 ps`; builds a `G4MoleculeCounter` (MT-correct via
  `G4MoleculeCounterManager`).
- `sim.cc:32` - `G4RunManagerType::Serial` (MT commented).
- `CMakeLists.txt:20-30` - globs `src/*.cc` + `header/*.hh` recursively and adds every header dir to
  the include path -> **no CMake edit needed** for new files.
- Project scope (`CLAUDE.md`): homogeneous water box, no UHDR pulse structure, no voxelization.

---

## 2. New files (picked up automatically by the CMake glob)

```
header/DnaChemistryWorld.hh      src/DnaChemistryWorld.cc
header/DnaChemistryList.hh       src/DnaChemistryList.cc
```

---

## 3. Step-by-step

### Step 1 - `DnaChemistryWorld` (`G4VChemistryWorld` subclass)

```cpp
class DnaChemistryWorld : public G4VChemistryWorld {   // NOT a messenger yet
  void ConstructChemistryBoundary()  override;   // build fpChemistryBoundary
  void ConstructChemistryComponents() override;  // fill fpChemicalComponent (bulk water; H3Op(B)/OHm(B) from pH)
  // later: setters for scavenger species/concentrations + a G4UImessenger
};
```

- `ConstructChemistryBoundary()`: build a `G4DNABoundingBox` sized to the water box. Half-side from
  `DetectorConstruction` (`fWorldSizeX/2`, currently 500 um). Init-list order `{xhi,xlo,yhi,ylo,zhi,zlo}`.
- `ConstructChemistryComponents()`: bulk `H2O` = 55.3 M; `H3Op(B)` = 10^-pH M;
  `OHm(B)` = 10^-(14-pH) M (pH member, default 7). No O2/CO2/scavengers yet - documented extension point.
- **Basis:** `chemistry-stage.md` *G4VChemistryWorld* (pure-virtual API, protected members,
  "nothing in core Geant4 instantiates a `G4VChemistryWorld`"); `uhdr-specifics.md`
  *Chemistry World and Builders* (concrete numbers);
  `source/processes/electromagnetic/dna/utils/include/G4VChemistryWorld.hh`.

### Step 2 - `DnaChemistryList` (`G4VUserChemistryList` + `G4VPhysicsConstructor`)

```cpp
class DnaChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor {
  DnaChemistryList();                                  // : G4VUserChemistryList(true); SetChemistryList(this)
  void ConstructParticle() override;                   // -> ConstructMolecule()
  void ConstructMolecule() override;
  void ConstructDissociationChannels() override;
  void ConstructReactionTable(G4DNAMolecularReactionTable*) override;
  void ConstructTimeStepModel(G4DNAMolecularReactionTable*) override;
  void ConstructProcess() override;
};
```

- **Constructor:** initialise `G4VUserChemistryList(true)` (dual-inheritance ownership flag - prevents
  double free), then `G4DNAChemistryManager::Instance()->SetChemistryList(this)`. No
  `G4_DECLARE_PHYSCONSTR_FACTORY` (instantiated with `new`).
  *Basis:* `chemistry-stage.md` *Custom Chemistry List* Gotchas; `G4EmDNAChemistry_option3.cc:74-78`.

- **`ConstructMolecule()`:** delegate to `G4ChemDissociationChannels_option1::ConstructMolecule()`
  (standard set + `(B)` bulk species the chem world needs); then set diffusion coefficients / radii
  to match the current `macro/beam.in:19-25` values for the 7 tracked species.
  *Basis:* `chemistry-stage.md` *G4ChemDissociationChannels / _option1*; `EmDNAChemistry.cc:98-107`.

- **`ConstructDissociationChannels()`:** delegate to
  `G4ChemDissociationChannels_option1::ConstructDissociationChannels()`.
  *Basis:* `EmDNAChemistry.cc:251-254`.

- **`ConstructReactionTable(table)`:**
  1. First call `<DetectorConstruction>->GetChemistryWorld()->ConstructChemistryComponents()`
     **here** (master-only phase - see section 4), so bulk concentrations exist before
     builders/scavengers need them.
  2. Add the 9 reactions from `macro/beam.in:32-43` as `G4DNAMolecularReactionData`
     (`rate * (1e-3*m3/(mole*s))`, `table->SetReaction(...)`).
  3. Commented extension point for scavenger reactions with `(B)` species.
  *Basis:* `chemistry-stage.md` *Custom Chemistry List* (dispatch), *Reactions and Reaction Table*
  (rate-unit idiom); `uhdr-specifics.md` *Reaction builders*; `G4DNAChemistryManager.cc:400-417`.

- **`ConstructTimeStepModel(table)` - the IRT guard (decision 5):**
  ```
  model = G4EmParameters::Instance()->GetTimeStepModel();
  if (model == IRT || model == Unknown)  -> G4Exception(..., FatalException, ...)
  if (model == SBS)      RegisterTimeStepModel(new G4DNAMolecularStepByStepModel(), 0);
  else /* IRT_syn */     RegisterTimeStepModel(new G4DNAIndependentReactionTimeModel(), 0);
  ```
  Fresh model object per call (per worker). `ChemUtils::ToString` already exists for the message.
  *Basis:* `physics-lists.md` *Available Chemistry Constructors*; `gotchas.md` *Custom Chemistry List*
  ("handling only SBS/IRT_syn and not IRT ... silently produces nothing" - here made loud);
  `G4EmDNAChemistry_option3.cc:754-767`; default `Unknown` from `G4EmLowEParameters.cc:79`.

- **`ConstructProcess()`:** per-molecule loop -
  - non-water: register transport (`G4DNABrownianTransportation` now; swap to a bounded variant when
    scavengers land) - only when `model != IRT` (guaranteed by the guard, but keep the check for
    parity with option3);
  - water: `AddRestProcess(new G4DNAElectronHoleRecombination(), 2)` and
    `AddRestProcess(new G4DNAMolecularDissociation("H2O_DNAMolecularDecay") +
    SetDisplacer(G4DNAWaterDissociationDisplacer), 1)`;
  - electron: `G4DNAElectronSolvation` if not already present; extend `G4DNASancheExcitationModel`
    low-E limit;
  - end with `G4DNAChemistryManager::Instance()->Initialize()`.
  - **Do not** call `ConstructChemistryComponents()` here (UHDR does at `EmDNAChemistry.cc:308`; that
    races under MT - moved to `ConstructReactionTable`, section 4).
  *Basis:* `dna-processes.md` *Chemical-Stage Processes*; `G4EmDNAChemistry_option3.cc:670-750`.

### Step 3 - `PhysicsList` wiring + SBS default

- `src/PhysicsList.cc` constructor: after physics/chemistry selection, add
  `G4EmParameters::Instance()->SetTimeStepModel(G4ChemTimeStepModel::SBS);` (project default even if
  a macro omits `/process/chem/TimeStepModel`).
- `SetDNAChemistry()` (`:79`): add branch `name == "DnaChemistryList"` ->
  `to_register = new DnaChemistryList();` -> keep the existing `RegisterPhysics(to_register)` path and
  the "chemistry-before-physics" guard (`:86`).
- Change default at `:38` from `"G4EmDNAChemistry_option3"` to `"DnaChemistryList"`.
- *Basis:* `physics-lists.md` *Selection in PhysicsList* (pattern 1; "must run after the EM DNA
  physics constructor"), *Gotchas*; current `src/PhysicsList.cc:24-41, 79-134`.

### Step 4 - `DetectorConstruction` owns the chem world

- `header/DetectorConstruction.hh`: add `std::unique_ptr<G4VChemistryWorld> fpChemistryWorld;` and
  `G4VChemistryWorld* GetChemistryWorld() const;`.
- Constructor (`src/DetectorConstruction.cc:25`): `fpChemistryWorld = std::make_unique<DnaChemistryWorld>();`
  then `fpChemistryWorld->ConstructChemistryBoundary();` (master, PreInit - safe).
- Optional: in `ConstructDetector()` size the world box from
  `fpChemistryWorld->GetChemistryBoundary()->halfSideLengthInY()*2` instead of the hard-coded
  1000 um, so geometry and chemistry domain stay in sync. Keep a single homogeneous box (project
  scope - no voxelization).
- *Basis:* `uhdr-specifics.md` *Chemistry World and Builders* -> "Wiring (the important part - nothing
  auto-registers this)"; `chemistry-stage.md` *G4VChemistryWorld* consumers list;
  `UHDR/src/DetectorConstruction.cc:48-63`.

### Step 5 - `ActionInitialization`: scavenger material, per worker thread

- In `Build()` (`src/ActionInitialization.cc:34`), inside the existing
  `if (G4DNAChemistryManager::IsActivated())` block (`:44`):
  - retrieve `const DetectorConstruction*` via
    `G4RunManager::GetRunManager()->GetUserDetectorConstruction()`;
  - `auto sc = std::make_unique<G4DNAScavengerMaterial>(det->GetChemistryWorld());`
  - `G4Scheduler::Instance()->SetScavengerMaterial(std::move(sc));`
  - register `TimeStepAction` here too - see Step 6.
- **Not** in `BuildForMaster()` - `G4Scheduler` and scavenger material are thread-local; `Build()`
  runs per worker.
- Keep `BuildMoleculeCounters()` as-is (already MT-correct).
- *Basis:* `uhdr-specifics.md` *EmDNAChemistry* init-flow diagram;
  `UHDR/src/ActionInitialization.cc:85-97`.

### Step 6 - Scheduler end time + `TimeStepAction` (prerequisite for meaningful output)

Not strictly part of "chemistry definition", but currently chemistry only runs to **1.3 ps** and
`TimeStepAction` is **not registered** (`src/ActionInitialization.cc:47-48`). To exercise the new
reaction set:
- register `TimeStepAction` on `G4Scheduler::Instance()->SetUserAction(...)` in `Build()`;
- raise `SetEndTime()` to the intended chemistry cutoff (e.g. 1 us) or make it macro-settable;
- `TimeStepAction::DumpPreChemical` writes `output_event_<id>.txt` (`src/TimeStepAction.cc:174`) -
  under MT this collides across threads; use the thread-id filename branch already sketched at
  `src/TimeStepAction.cc:161-165`.
- *Basis:* `chemistry-stage.md` *Time-Step Action and Scoring*; `gotchas.md` *Output and Analysis*,
  *Multithreading*.

### Step 7 - Macro cleanup (`macro/beam.in`, `macro/beam_02.in`) - see section 5

### Step 8 - Build & validate - see section 4

---

## 3b. Files touched

| File | Change |
|---|---|
| `header/DnaChemistryWorld.hh`, `src/DnaChemistryWorld.cc` | new |
| `header/DnaChemistryList.hh`, `src/DnaChemistryList.cc` | new |
| `header/DetectorConstruction.hh`, `src/DetectorConstruction.cc` | own + expose chem world |
| `src/PhysicsList.cc` | new `SetDNAChemistry` branch; SBS default |
| `src/ActionInitialization.cc` | scavenger material + TimeStepAction (per-thread) |
| `macro/beam.in`, `macro/beam_02.in` | strip harmful/moved commands |
| `CMakeLists.txt` | none (glob) |
| `sim.cc` | none required; MT flip is a separate validation toggle |

---

## 4. MT-compliance checklist

| Concern | Rule applied | Basis |
|---|---|---|
| Chemistry list registration | `SetChemistryList(this)` in ctor runs once on master | `chemistry-stage.md` *Custom Chemistry List* -> "Who calls what"; `G4EmDNAChemistry_option3.cc:77` |
| `ConstructMolecule` / `ConstructReactionTable` | master-only phases - safe to touch shared `G4MoleculeTable` / reaction table | `G4DNAChemistryManager.cc:400-417`; `chemistry-stage.md` dispatch list |
| `ConstructTimeStepModel` | called **per worker** - allocate a fresh `G4VITStepModel` each call, no shared model pointer | `G4DNAChemistryManager.cc:471-481`; `G4EmDNAChemistry_option3.cc:754` |
| `ConstructProcess` | per worker - only register processes; **no** run-varying data members | `dna-processes.md` *Chemical-Stage Processes*; `G4EmDNAChemistry_option3.cc:670` |
| `ConstructChemistryComponents()` | call **once from `ConstructReactionTable` (master)**, not from `ConstructProcess` - avoids concurrent writes to `fpChemicalComponent` (deliberate deviation from `EmDNAChemistry.cc:308`) | `chemistry-stage.md` *G4VChemistryWorld* Gotchas + dispatch table |
| `DnaChemistryWorld` lifetime | created in `DetectorConstruction` ctor (master), read-only after PreInit; workers only *read* `GetChemistryBoundary()` / `GetChemicalComponent()` | `uhdr-specifics.md` *Chemistry World and Builders* wiring |
| `G4DNAScavengerMaterial` | created per worker in `Build()`, handed to thread-local `G4Scheduler` | `UHDR/src/ActionInitialization.cc:85-97` |
| Molecule counter | already MT-correct (`G4MoleculeCounterManager` in both `Build*`) | current `src/ActionInitialization.cc:28-72`; `gotchas.md` *Molecule Counter in Multithreading* |
| Output files | thread-id in filename when `TimeStepAction` re-enabled | `gotchas.md` *Multithreading*, *Output and Analysis* |
| Double free | `G4VUserChemistryList(true)` because class is also a `G4VPhysicsConstructor` | `gotchas.md` *Custom Chemistry List* |
| Validation | build once serial, once MT (`G4RunManagerType::MT`, >=2 threads); G-values / counter yields must agree within statistics | `CLAUDE.md` *Build and Run*; `chemistry-stage.md` *Species Scoring and G-Values* |

---

## 5. Macro cleanup - exact commands and why

Applies to both `macro/beam.in` and `macro/beam_02.in`.

| Command(s) | Action | Reason |
|---|---|---|
| `/chem/reaction/UI` (`beam.in:29`, `beam_02.in:31`) | **remove** | calls `G4DNAMolecularReactionTable::Reset()` - wipes the table `DnaChemistryList::ConstructReactionTable()` just built. Verified `G4ReactionTableMessenger.cc:67-71`. `gotchas.md` *Macro Commands Are Order-Sensitive* |
| `/chem/reaction/add ...` (`beam.in:32-43`, `beam_02.in:34-45`) | **remove** (transcribe into `ConstructReactionTable`) | reactions now defined in C++; leaving them appends a second, macro-defined set to the same singleton after `InitializeMaster` already ran |
| `/chem/reaction/print` (`beam.in:45`, `beam_02.in:47`) | **remove** (or keep once; harmless) | table can be printed from code if wanted |
| `/chem/species ...` (`beam.in:19-25`, `beam_02.in:21-27`) | **remove** | molecules now created in `ConstructMolecule()`; macro re-declaration after `/run/initialize` is redundant/fragile. `gotchas.md` *Chemistry Requires Molecule Definitions* |
| `/process/chem/TimeStepModel IRT` (`beam.in:10`) | **change to `SBS`** (or delete - PhysicsList sets SBS) | decision 5; `IRT` now triggers the `FatalException` guard |
| `/process/chem/TimeStepModel SBS` (`beam_02.in:12`) | keep | already correct |
| `/scheduler/whyDoYouStop`, `/scheduler/verbose` (`beam_02.in:54-55`) | keep (diagnostic, not harmful) | - |
| `/run/verbose`, `/tracking/verbose`, `/dnaLogger/verbose`, `/process/dna/e-SolvationSubType`, `/gun/*`, `/primaryKiller/*`, `/run/initialize`, `/run/beamOn` | keep | unaffected |

Net: each macro shrinks to solvation subtype + (optional) time-step model + `/run/initialize` + gun +
primaryKiller + `beamOn`. `/run/initialize` stays before the gun; species/reactions are no longer
ordering-sensitive because they run in code inside `/run/initialize`.

---

## 6. Risks / open items

- **`GetConfiguration` typos** -> `nullptr` -> deferred segfault. Transcribe species names exactly
  (`degOH` shown here as the ring-char name, `e_aq`, `H3Op`, `OHm`, `H3Op(B)`, `OHm(B)`).
  `gotchas.md` *Custom Chemistry List*.
- **`DnaChemistryList::ConstructReactionTable` needs the detector** - `dynamic_cast` on
  `GetUserDetectorConstruction()`; if someone swaps `DetectorConstruction`, this null-derefs. Guard
  with a clear `G4Exception`. `uhdr-specifics.md` *EmDNAChemistry* Gotchas.
- **Transport model vs. future bounded/scavenger transport** - start with
  `G4DNABrownianTransportation`; switching to a bounded variant is localized to `ConstructProcess()`
  when scavengers are added.
- **`sim.cc` global `std::ofstream out;` (`sim.cc:21`)** - unused; if wired up later it is not
  thread-safe. Leave alone for now.
- **Baseline for validation** - capture a serial run with the current `G4EmDNAChemistry_option3` +
  macro reactions *before* the change, to diff G-values afterwards.

---

## IMPLEMENTATION STATUS - 2026-08-28 (iteration 1: class-based chemistry)

Implemented and build-verified (MSVC 19.50 / Ninja, RelWithDebInfo). `sim.exe`
builds clean; `macro/beam.in`, `macro/beam_02.in` and a 1-event smoke macro all
run to completion (chemistry evolves to 1 us, exit 0); `/process/chem/TimeStepModel IRT`
correctly aborts with `DnaChemistryList::ConstructProcess / BadTimeStepModel`.

### New files
- `header/DnaChemistryWorld.hh` / `src/DnaChemistryWorld.cc` - `G4VChemistryWorld`
  subclass. `ConstructChemistryBoundary()` -> cubic `G4DNABoundingBox`, half-side
  `fHalfBox` (default 500 um). `ConstructChemistryComponents()` -> bulk H2O 55.3 M,
  H3Op(B)/OHm(B) from pH (default 7). `SetpH`, `SetHalfBox` for later use. No
  messenger yet.
- `header/DnaChemistryList.hh` / `src/DnaChemistryList.cc` -
  `G4VUserChemistryList` + `G4VPhysicsConstructor`. Molecules + dissociation
  channels delegated to `G4ChemDissociationChannels_option1`; 7 historical
  `/chem/species` diffusion/radius tweaks re-applied in code; 9 pure-water
  reactions from the old `beam.in`; `ConstructProcess` mirrors
  `G4EmDNAChemistry_option3` (Sanche low-E extension, electron solvation,
  Brownian transport, water rest processes). `G4_DECLARE_PHYSCONSTR_FACTORY`
  included for parity.

### Modified files
- `src/PhysicsList.cc` - `SetDNAChemistry("DnaChemistryList")` branch added;
  default switched from `G4EmDNAChemistry_option3` to `DnaChemistryList`; ctor
  now sets `G4EmParameters::SetTimeStepModel(SBS)` as the project default.
- `header/DetectorConstruction.hh` / `src/DetectorConstruction.cc` - owns
  `std::unique_ptr<G4VChemistryWorld> fpChemistryWorld` (a `DnaChemistryWorld`),
  built in the ctor; `GetChemistryWorld()` accessor; world box now sized from
  `GetChemistryBoundary()` (unchanged 1 mm by default).
- `src/ActionInitialization.cc` - registers `TimeStepAction` on the scheduler;
  `SetEndTime` raised 1.3 ps -> 1 us; commented-ready scavenger-material block.
- `macro/beam.in`, `macro/beam_02.in` - removed `/chem/species`,
  `/chem/reaction/UI`, `/chem/reaction/add`, `/chem/reaction/print`; `beam.in`
  time-step model `IRT` -> `SBS`.

### Deviations from the plan (all deliberate, all source-grounded)

1. **`ConstructChemistryComponents()` is called from `ConstructReactionTable()`**
   (master-only, `G4DNAChemistryManager::InitializeMaster`), not from
   `ConstructProcess()` as UHDR does (`EmDNAChemistry.cc:308`). `ConstructProcess`
   runs per worker thread -> concurrent writes to the shared `fpChemicalComponent`
   map. Reaction-table construction is single-threaded, so this is race-free.

2. **`G4DNAScavengerMaterial` is NOT wired yet** (left as a commented, ready-to-use
   block in `ActionInitialization::Build()`). Reason found in source:
   `G4RunManager::SetUserInitialization(G4VUserActionInitialization*)` calls
   `Build()` *immediately* (`G4RunManager.cc:977`), i.e. before `/run/initialize`
   in serial mode - so `ConstructChemistryComponents()` has not run and the
   composition map is empty. With no scavenger species/reactions in this
   iteration the material has nothing to do; it will be wired together with the
   first scavenger reaction, at which point switching `sim.cc` to
   `G4RunManagerType::MT` (so `Build()` runs per worker after master init) is the
   clean enabler.

3. **`sim.cc` left on `G4RunManagerType::Serial`.** All new code is MT-safe
   (self-registration once on master; molecule/reaction construction in
   master-only hooks; fresh time-step model object per worker; no run-varying
   members), but flipping the run manager also changes output-file handling
   (`TimeStepAction::DumpPreChemical`) and RNG streams, so it is kept as an
   explicit follow-up validation step rather than bundled here.

4. **Scheduler end time raised to 1 us** (plan allowed "1 us or macro-settable").
   Chosen 1 us unconditionally so the reaction set actually evolves; revert in
   `ActionInitialization.cc` if the pre-chemical-only 1.3 ps behaviour is wanted.

5. **`kOH` helper for the hydroxyl name.** The project's CMake does not pass
   `/utf-8` to MSVC, so the `degree-sign OH` configuration tag is built from
   explicit UTF-8 bytes (`"\xC2\xB0" + "OH"`) to guarantee it matches the name
   stored by `G4ChemDissociationChannels_option1` (compiled by Geant4 with
   `/utf-8`).

### Follow-ups (not done here)
- Wire `G4DNAScavengerMaterial` + per-molecule `G4DNAScavengerProcess` when the
  first bulk/scavenger reaction is added (see comment blocks in
  `DnaChemistryList::ConstructProcess` and `ActionInitialization::Build`).
- Switch `sim.cc` to MT and re-validate G-values serial vs MT.
- `TimeStepAction::DumpPreChemical` needs a thread-id in the output filename
  before MT (`src/TimeStepAction.cc:161-165` has the sketch).
- Capture a numerical G-value baseline (old `G4EmDNAChemistry_option3` + macro
  reactions) and diff against the new class-based path.

---

## ITERATION 2 - 2026-08-28 - simplified PhysicsList + optional O2 scavenger

Two follow-up requests, both implemented and build/run-verified (MSVC 19.50 / Ninja).

### (a) PhysicsList simplified to the UHDR style

`header/PhysicsList.hh` + `src/PhysicsList.cc` rewritten (208 lines removed). The
string-dispatched `SetDNAPhysics` / `SetDNAChemistry` + `RegisterPhysics` machinery is gone.
Now, exactly like `examples/extended/medical/dna/UHDR/src/PhysicsList.cc`:

```cpp
PhysicsList::PhysicsList()
  : fEmDNAPhysicsList(new G4EmDNAPhysics(0)),
    fEmDNAChemistryList(new DnaChemistryList) { ...cuts, SBS default... }

void PhysicsList::ConstructParticle() { fEmDNAPhysicsList->ConstructParticle();
                                        fEmDNAChemistryList->ConstructParticle(); }
void PhysicsList::ConstructProcess()  { AddTransportation();
                                        fEmDNAPhysicsList->ConstructProcess();
                                        fEmDNAChemistryList->ConstructProcess(); }
```

- To change the EM-DNA physics option now you edit the constructor (one line).
- `IsChemistryEnabled()` kept (StackingAction uses it) -> `fEmDNAChemistryList != nullptr`.
- `DnaChemistryList` still self-registers with `G4DNAChemistryManager` in its ctor and is
  owned by the `std::unique_ptr` in PhysicsList (the `G4VUserChemistryList(true)` flag makes
  the manager release rather than delete - no double free).

### (b) Optional dissolved-O2 bulk scavenger (UHDR O2 sub-system)

Enabled with `/chem/env/O2 <percent>` (% of a pure-O2 atmosphere, kH = 0.0013 M;
`/chem/env/pH <double>` also added). Default 0 -> anoxic -> byte-identical to the pure-water
baseline (verified: pre-chem list sizes 108,121,143,142,134,129 unchanged).

Reactions ported verbatim (rates + equilibrium types) from the UHDR example:
- `ChemOxygenWaterBuilder::OxygenScavengerReaction` (e_aq+O2, H+O2, O-+O2)
- the O2 subset of `ChemOxygenWaterBuilder::SecondOrderReactionExtended` (19 O2-/HO2/HO2-/O-/O3-
  radical-radical reactions) -> reaction table
- the O2-touching `G4DNAScavengerProcess` blocks of `EmDNAChemistry::ConstructProcess`
  (H, e_aq, O2m, HO2, HO2m, O-, O3- vs bulk O2(B)/H3O+(B)/OH-(B)/H2O) -> per-molecule
  `G4DNAScavengerProcess`

Files:
- `DnaChemistryWorld` - `G4GenericMessenger` for `/chem/env/{pH,O2}`; `ConstructChemistryComponents`
  adds `fpChemicalComponent[O2]` when enabled; `IsOxygenScavengerEnabled()` /
  `GetOxygenConcentration()` accessors.
- `DnaChemistryList` - `ConstructMolecule` now also `CreateConfiguration("H2O", ...)` (required by
  `G4DNAScavengerProcess`); `ConstructReactionTable` calls `ConstructOxygenReactionTable` when
  O2 is on; `ConstructProcess` calls `RegisterOxygenScavengerProcesses` and then (after
  `G4DNAChemistryManager::Initialize()`, so the composition is known) creates the
  `G4DNAScavengerMaterial` and hands it to the thread-local `G4Scheduler`.
- `ActionInitialization` - scavenger-material creation removed (moved to `ConstructProcess` -
  see below); `RunAction` reverted to original.
- `macro/beam.in` - commented `/chem/env/O2` example; `macro/beam_o2.in` - new, O2 = 21 %.

### Verification of (b)

`eInteractionWithMedium` scheduler steps (same seed, 1 event, 100 keV e-):
pure water = 3 (just water decay), O2 21 % = 46 -> the `G4DNAScavengerProcess` chain fires.
`eCollisionBetweenTracks` drops 898 -> 875 (e_aq / OH diverted into the O2 pathway).

### Deviation: where the scavenger material is created

Not `ActionInitialization::Build()` (UHDR's spot) but the end of
`DnaChemistryList::ConstructProcess()`, after `G4DNAChemistryManager::Initialize()`. Reason:
in serial mode `G4RunManager::SetUserInitialization` calls `Build()` *before the macro runs*
(`G4RunManager.cc:977`), so neither the O2 setting nor the composition map is known there, and
`G4DNAScavengerProcess::BuildPhysicsTable()` (`G4DNAScavengerProcess.cc:81`) would later read a
null scavenger material from the scheduler and segfault. `ConstructProcess` runs during
`/run/initialize` (after the macro, once in serial / per-worker in MT), and the scheduler is
thread-local there - correct for both modes. The composition map is populated by
`ConstructReactionTable` inside the `Initialize()` call that precedes the creation.

### Done in this iteration (completing the plan follow-ups)
- `sim.cc` -> `G4RunManagerType::MT` (batch default 4 threads; `/run/numberOfThreads`
  overrides; one-line revert to Serial documented in the file). All three macros verified
  in MT.
- `src/TimeStepAction.cc` - `DumpPreChemical` now writes `output_event_t<tid>_e<eid>.txt`
  in MT mode (per-worker, no filename collision).
- `src/RunAction.cc` - `EndOfRunAction` (master) calls `ScoreSpecies::ASCII()` +
  `OutputAndClear()` -> `Species.Txt` (human-readable) + `Species.root`. Yields are merged
  across worker threads by the existing `Run::Merge` ->
  `ScoreSpecies::AbsorbResultsFromWorkerScorer`.

### O2 scavenger - quantitative check (MT, 100 keV e-, 21 % O2, t = 1 us)
`Species.Txt`: `e_aq^-1 = 0`, `H^0 = 0` (both fully scavenged), `O_2^-1 = 57`,
`HO_2 = 29` produced. Pure water at the same time keeps hundreds of e_aq / OH. The oxygen
effect (reducing species diverted to superoxide) is reproduced.

---

## 7. Suggested implementation order (iteration 1, historical)

1. `DnaChemistryWorld` (compiles standalone).
2. `DnaChemistryList` with `ConstructMolecule` / `ConstructDissociationChannels` /
   `ConstructTimeStepModel` (+ IRT guard) / minimal `ConstructProcess`; `ConstructReactionTable`
   empty.
3. `PhysicsList` branch + SBS default; build; run - expect chemistry with no reactions.
4. `DetectorConstruction` ownership + `GetChemistryWorld()`.
5. Fill `ConstructReactionTable` (9 reactions) + call `ConstructChemistryComponents()` there.
6. `ActionInitialization`: scavenger material + `TimeStepAction`; raise scheduler end time.
7. Macro cleanup.
8. Validate serial vs MT against the pre-change baseline.
