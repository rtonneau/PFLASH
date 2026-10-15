# Add a new Chemistry

## What this is

How to add a new named **Chemistry** (reaction table + bulk-reaction list,
and optionally extra molecules) that `/chem/select <name>` can pick, using
the project's existing builders as the pattern. Rationale for packaging
reaction content this way, and why the acid-base buffer is per-Chemistry
rather than unconditional, is
[`docs/adr/0002-named-chemistries.md`](../adr/0002-named-chemistries.md) —
not repeated here.

## Steps

1. **Copy a `catalog/` builder.** `src/chemistry/catalog/PureWaterReactions.{hh,cc}`
   is the reference (`header/chemistry/catalog/PureWaterReactions.hh:5-7`:
   "the reference to copy when adding another one"); `BoscoloChemReactions.{hh,cc}`
   is a smaller worked example (26 reactions, no acid-base buffer). Copy both
   files, rename the pair, and adjust the include-path prefix — each file is
   deliberately self-contained (no `DnaChemistryWorld`, `DnaLogger`, or other
   dnachem-min-specific include; only `chemistry/ChemistryTypes.hh` plus
   standard Geant4/Geant4-DNA headers), so it is copy-paste portable to
   another Geant4-DNA project (`PureWaterReactions.hh:9-11`).

2. **Implement `BuildXxxReactions`/`BuildXxxBulkReactions`.** Verified against
   `header/chemistry/catalog/PureWaterReactions.hh:47-63`, the two functions
   a Chemistry must provide are:

   ```cpp
   namespace PureWaterReactions
   {
     void BuildPureWaterReactions(G4DNAMolecularReactionTable* reactionTable);
     ChemistryTypes::BulkReactionList BuildPureWaterBulkReactions();
   }
   ```

   - `BuildXxxReactions(reactionTable)` populates the ordinary (non-bulk)
     reaction table. It looks up each species by name via
     `G4MoleculeTable::Instance()->GetConfiguration(name)`
     (`PureWaterReactions.cc:27-35`, a `Conf()` helper that raises a fatal
     `G4Exception` on an unknown name) and calls `reactionTable->SetReaction(...)`
     on a `new G4DNAMolecularReactionData(rate, a, b)` with
     `AddProduct()` for each product (`PureWaterReactions.cc:40-62`). It
     must run after the molecule set exists, i.e. after
     `G4ChemDissociationChannels_option1::ConstructMolecule()`
     (`PureWaterReactions.hh:52-53`).
   - `BuildXxxBulkReactions()` returns a `ChemistryTypes::BulkReactionList`
     (`header/chemistry/ChemistryTypes.hh:16-33`) as plain data — a vector of
     `BulkReactionEntry{molecule, reactions}`, each `BulkReaction{partner,
     rate, products, reactionType}` — describing reactions of a tracked
     molecule against a bulk partner (an acid-base species `"H3Op(B)"`/
     `"OHm(B)"`/`"H2O"`, or a scavenger such as `"O2"`). This list may be
     empty — `BoscoloChemReactions` has no acid-base buffer at all
     (`BoscoloChemReactions.hh:19-21`: "No acid-base buffer, by design... so
     `/chem/env/pH` has no chemical effect under this Chemistry"). The driver
     (`DnaChemistryList`) turns each entry into one `G4DNAScavengerProcess`
     per molecule for the particle-based stage, and also adds every bulk
     reaction to the reaction table for the mesoscopic stage.
   - Respect `ChemistryTypes::PartialReactionsEnabled()`
     (`ChemistryTypes.hh:40-55`) at the point a reaction would become Type
     II/IV (partially diffusion-controlled): call `SetReactionType(1)` only
     when it reads `true`; under SBS the driver sets it `false` and the
     reaction must stay type 0 (fully diffusion-controlled) — this call
     cannot be undone once made, so never make it unconditionally.
   - Optionally supply a third function pointer, `constructMolecules`,
     for a Chemistry that needs a molecule beyond the shared set (see
     `Tonneau2025Reactions::ConstructTonneau2025Molecules` for a worked
     example, the `HO3` extra molecule, [ADR
     0007](../adr/0007-extra-molecules-per-chemistry.md)). It "runs on the
     master thread from `DnaChemistryList::ConstructMolecule` and from
     `/chem/select` (`ChemistrySelectMessenger`), so an implementation must
     be idempotent (never `new G4MoleculeDefinition` for a name that may
     already exist)" (`header/chemistry/ChemistryRegistry.hh:25-28`).

3. **Register it in `src/chemistry/BuiltInChemistries.cc`.** Add one `add({...})`
   call inside `BuiltInChemistries::Register()`, naming the Chemistry and
   passing its two (or three) function pointers, following the existing
   entries:

   ```cpp
   add({"BoscoloChem", &BoscoloChemReactions::BuildBoscoloChemReactions,
        &BoscoloChemReactions::BuildBoscoloChemBulkReactions});
   add({"Tonneau2025", &Tonneau2025Reactions::BuildTonneau2025Reactions,
        &Tonneau2025Reactions::BuildTonneau2025BulkReactions,
        &Tonneau2025Reactions::ConstructTonneau2025Molecules});
   ```

   (`BuiltInChemistries.cc:32-38`) — include the new catalog header at the
   top of the file alongside the others, and nowhere else: `Register()` is
   the single place that knows the full built-in list.

4. **Select it with `/chem/select <name>`.** PreInit, before
   `/run/initialize`; case-insensitive; an unknown name is fatal, and
   picking a second, different name after one is already selected is also
   fatal (see the `ChemistryRegistry` contract below). `/chem/list` prints
   every registered name. A macro that exercises a new Chemistry typically
   looks like `beam_boscolo.in` or `beam_tonneau2025.in`: `/chem/select
   <Name>` then the usual gun/run commands.

## The `ChemistryRegistry` contract

Verified against `src/chemistry/ChemistryRegistry.cc` and
`header/chemistry/ChemistryRegistry.hh`:

- **Storage.** A `Chemistry` is `{name, buildReactions, buildBulkReactions,
  constructMolecules = nullptr}` — name plus plain function pointers, no
  object state (`ChemistryRegistry.hh:19-30`). `Register()` pushes a copy
  into a function-local `static State state;` (`ChemistryRegistry.cc:17-21`,
  `Instance()`) — one table for the whole process, not per-thread storage
  (see Serial vs MT below for why that is safe here).
- **`Register(chemistry, err)`** fails (`false` + `err`, no exception) for an
  empty name, a null `buildReactions`/`buildBulkReactions` pointer, or a name
  already registered — matched case-insensitively via a local `Lower()`
  transform (`ChemistryRegistry.cc:54-70`). `BuiltInChemistries::Register()`
  turns a rejected registration into a fatal `G4Exception`
  (`BuiltInChemistries.cc:22-28`) — `ChemistryRegistry` itself never raises
  one; it only reports `bool` + string (`ChemistryRegistry.hh:4-6`: "Pure
  logic: no Geant4 kernel, no logging, no process exit").
- **`Select(name, err)`** looks the name up case-insensitively
  (`Find()`/`Lower()`, same helper). Unknown name: `false` + an error
  listing every registered name (`JoinNames()`, `ChemistryRegistry.cc:72-78`,
  `42-49`). A *different* name than one already selected: also `false`, with
  an error naming both (`ChemistryRegistry.cc:80-84`) — this is what makes
  two different `/chem/select` calls in one process fatal (the messenger
  layer, `ChemistrySelectMessenger`, turns the `false` into a
  `G4Exception`). Selecting the **same** name again (e.g. the MT broadcast
  replay — see below) returns `true` and changes nothing
  (`ChemistryRegistry.cc:79-86`).
- **`Selected()`** returns the picked entry, or the entry named
  `kDefaultName` (`"PureWater"`) if nothing was ever selected, or `nullptr`
  if even that is not registered (`ChemistryRegistry.cc:89-93`).
- **`BuiltInChemistries::Register()` is idempotent** by its own `static bool
  done` guard (`BuiltInChemistries.cc:16-20`), independent of
  `ChemistryRegistry::Register`'s duplicate-name rejection — the two
  idempotency mechanisms exist at different layers and both matter: see
  Serial vs MT below for why `Register()` being callable more than once
  safely is relied upon.

## Serial vs MT

The question this recipe needs answered precisely: does
`DnaChemistryList`'s constructor — which calls `fSelectMessenger =
std::make_unique<ChemistrySelectMessenger>()` and, transitively through
`BuiltInChemistries::Register()`, populates `ChemistryRegistry`'s static
table — run once per process, or once per worker thread under MT? This is a
Geant4-kernel question, not a project one, so it was checked against the
kernel source rather than assumed from general MT familiarity with "physics
lists are built per thread."

1. `sim.cc` constructs the project's `PhysicsList` **exactly once**,
   regardless of Serial or MT: `runManager->SetUserInitialization(new
   PhysicsList);` (`sim.cc:144`) runs on the master thread only, before any
   worker exists. `PhysicsList`'s constructor in turn does `fEmDNAChemistryList(new
   DnaChemistryList)` (`src/physics/PhysicsList.cc:15-21`) — so there is
   exactly one `DnaChemistryList` C++ object for the whole process, built
   before `G4RunManager::Initialize()`/thread creation, in both Serial and
   MT.
2. In MT mode, the kernel does **not** construct a second `PhysicsList` (or
   `DnaChemistryList`) per worker. `G4MTRunManagerKernel::StartThread` reads
   the master's existing pointer and hands the *same* instance to the new
   worker's run manager:

   ```cpp
   // geant4SourcePath/source/run/src/G4MTRunManagerKernel.cc:187-191
   // Set the detector and physics list to the worker thread. Share with master
   const G4VUserDetectorConstruction* detector = masterRM->GetUserDetectorConstruction();
   wrm->G4RunManager::SetUserInitialization(const_cast<G4VUserDetectorConstruction*>(detector));
   const G4VUserPhysicsList* physicslist = masterRM->GetUserPhysicsList();
   wrm->SetUserInitialization(const_cast<G4VUserPhysicsList*>(physicslist));
   ```

   `G4TaskRunManagerKernel::StartThread`, the other MT backend
   `G4RunManagerFactory` can select, does the identical thing, with the
   identical comment
   (`geant4SourcePath/source/run/src/G4TaskRunManagerKernel.cc:195-199`).
   `G4WorkerRunManager::SetUserInitialization(G4VUserPhysicsList*)` then only
   calls `pl->InitializeWorker()` on that shared pointer and stores it
   (`geant4SourcePath/source/run/src/G4WorkerRunManager.cc:589-593`) — no
   construction. So **the C++ object is shared across every worker
   thread**, not rebuilt: `DnaChemistryList`'s constructor, and therefore
   `BuiltInChemistries::Register()`, run exactly once per process in both
   Serial and MT.

   (What *does* run per worker thread on that one shared object is
   `ConstructParticle()`/`ConstructProcess()` — called once per worker by
   each worker's own `G4RunManagerKernel::InitializePhysics()`
   (`G4RunManagerKernel.cc:516,578`) — because those methods build
   thread-local Geant4-kernel state, molecule/process tables, that the
   kernel itself keeps thread-local internally. The *constructor*, where
   `ChemistrySelectMessenger` is created and `BuiltInChemistries::Register()`
   is called, is not one of those methods and is not re-invoked.)
3. Consequence for `ChemistryRegistry`'s non-thread-synchronized static table
   (`header/chemistry/ChemistryRegistry.hh:4-6`): it is safe precisely
   *because* there is only ever one `DnaChemistryList`/registration call, not
   because of any locking. `BuiltInChemistries::Register()`'s `static bool
   done` guard (point 4 above) is a second, independent safety net — relied
   on because `ChemistrySelectMessenger` is also the single, shared instance
   that every worker's broadcast replay of `/chem/select <name>` (see
   [`create-a-macro-command.md`](create-a-macro-command.md)'s own Serial vs
   MT section for the replay mechanism) ends up calling: the *same*
   `SetNewValue` on the *same* object, re-running `ChemistryRegistry::Select`
   with the same name on every worker. That is harmless only because
   `Select` on an already-matching name returns `true` and changes nothing
   (point 2 of the contract above).

**What this implies for a Chemistry author:** `BuildXxxReactions`/
`BuildXxxBulkReactions`/`constructMolecules` are called from exactly one
object's exactly-once construction path and from `/chem/select` (also
effectively once per process, modulo the harmless same-name replay above) —
never concurrently and never once per worker. There is no need to make a
Chemistry's builder functions thread-safe against concurrent calls from
different workers; the one documented hazard is re-entrancy within a single
process (hence `constructMolecules` must be idempotent, point in step 2
above), not cross-thread races.

## See also

- [`docs/adr/0002-named-chemistries.md`](../adr/0002-named-chemistries.md) —
  why reaction content is packaged as a named, selectable Chemistry and why
  the acid-base buffer lives per-Chemistry.
- [`docs/adr/0007-extra-molecules-per-chemistry.md`](../adr/0007-extra-molecules-per-chemistry.md)
  — the `constructMolecules` hook, worked example `Tonneau2025`'s `HO3`.
- [`create-a-macro-command.md`](create-a-macro-command.md) — the `/chem/select`
  messenger pattern (`ChemistrySelectMessenger`, a plain `G4UImessenger`
  attaching to the pre-existing `/chem/` directory) and the MT broadcast
  mechanism that replays a PreInit command like `/chem/select` on every
  worker thread.
- [`dnachem-min-macro-command-reference.md`](dnachem-min-macro-command-reference.md)
  — `/chem/select` and `/chem/list` alongside every other macro command.
