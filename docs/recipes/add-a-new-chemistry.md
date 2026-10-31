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

1. **Copy a `catalog/` builder.** `src/chemistry/catalog/PureWaterReactions.cc` and
   `header/chemistry/catalog/PureWaterReactions.hh` are the reference pair.
   `BoscoloChemReactions.{hh,cc}` in the matching source/header directories
   is a smaller worked example (26 reactions, no acid-base buffer). Copy both
   files, rename the pair, and adjust the include-path prefix — each file is
   deliberately self-contained (no `DnaChemistryWorld`, `DnaLogger`, or other
   dnachem-min-specific include; only `chemistry/ChemistryTypes.hh` plus
   standard Geant4/Geant4-DNA headers), so it is copy-paste portable to
   another Geant4-DNA project (`PureWaterReactions.hh`).

2. **Implement `BuildXxxReactions`/`BuildXxxBulkReactions`.** Verified against
   `header/chemistry/catalog/PureWaterReactions.hh`, the two functions
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
     (`PureWaterReactions.cc`, a `Conf()` helper that raises a fatal
     `G4Exception` on an unknown name) and calls `reactionTable->SetReaction(...)`
     on a `new G4DNAMolecularReactionData(rate, a, b)` with
     `AddProduct()` for each product (`PureWaterReactions.cc`). It
     must run after the molecule set exists, i.e. after
     `G4ChemDissociationChannels_option1::ConstructMolecule()`
     (`PureWaterReactions.hh`).
   - `BuildXxxBulkReactions()` returns a `ChemistryTypes::BulkReactionList`
     (`header/chemistry/ChemistryTypes.hh`) as plain data — a vector of
     `BulkReactionEntry{molecule, reactions}`, each `BulkReaction{partner,
     rate, products, reactionType}` — describing reactions of a tracked
     molecule against a bulk partner (an acid-base species `"H3Op(B)"`/
     `"OHm(B)"`/`"H2O"`, or a scavenger such as `"O2"`). This list may be
     empty — `BoscoloChemReactions` has no acid-base buffer at all
     (`BoscoloChemReactions.hh`: "No acid-base buffer, by design... so
     `/chem/env/pH` has no chemical effect under this Chemistry"). The driver
     (`DnaChemistryList`) turns each entry into one `G4DNAScavengerProcess`
     per molecule for the particle-based stage, and also adds every bulk
     reaction to the reaction table for the mesoscopic stage.
   - Respect `ChemistryTypes::PartialReactionsEnabled()`
     (`ChemistryTypes.hh`) at the point a reaction would become Type
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
     already exist)" (`header/chemistry/ChemistryRegistry.hh`).

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

   (`BuiltInChemistries.cc`) — include the new catalog header at the
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
  object state (`ChemistryRegistry.hh`). `Register()` pushes a copy
  into a function-local `static State state;` (`ChemistryRegistry.cc`,
  `Instance()`) — one table for the whole process, not per-thread storage
  (see Serial vs MT below for the required initialization discipline).
- **`Register(chemistry, err)`** reports validation failure (`false` + `err`) for an
  empty name, a null `buildReactions`/`buildBulkReactions` pointer, or a name
  already registered — matched case-insensitively via a local `Lower()`
  transform (`ChemistryRegistry.cc`). `BuiltInChemistries::Register()`
  turns a rejected registration into a fatal `G4Exception`
  (`BuiltInChemistries.cc`) — `ChemistryRegistry` itself never raises
  one; it only reports `bool` + string (`ChemistryRegistry.hh`: "Pure
  logic: no Geant4 kernel, no logging, no process exit").
- **`Select(name, err)`** looks the name up case-insensitively
  (`Find()`/`Lower()`, same helper). Unknown name: `false` + an error
  listing every registered name (`JoinNames()` in `ChemistryRegistry.cc`). A *different* name than one already selected: also `false`, with
  an error naming both (`ChemistryRegistry.cc`) — this is what makes
  two different `/chem/select` calls in one process fatal (the messenger
  layer, `ChemistrySelectMessenger`, turns the `false` into a
  `G4Exception`). Selecting the **same** name again before initialization returns `true`
  and preserves the selection value
  (`ChemistryRegistry.cc`).
- **`Selected()`** returns the picked entry, or the entry named
  `kDefaultName` (`"PureWater"`) if nothing was ever selected, or `nullptr`
  if even that is not registered (`ChemistryRegistry.cc`).
- **`BuiltInChemistries::Register()` is idempotent** by its own `static bool
  done` guard (`BuiltInChemistries.cc`), independent of
  `ChemistryRegistry::Register`'s duplicate-name rejection — the registry rejects duplicates while the built-in helper
  skips later calls. Neither mechanism synchronizes concurrent mutation.

## Serial vs MT

`sim.cc` constructs one `PhysicsList`, whose constructor creates one
`DnaChemistryList`. Geant4 shares the physics-list object with its workers
(`source/run/src/G4MTRunManagerKernel.cc`, `StartThread`, lines 187–191).
`G4WorkerRunManager::SetUserInitialization` calls `InitializeWorker` on that
object; it does not construct another project physics list.

Object construction and builder invocation are different lifecycles:

- `BuiltInChemistries::Register` runs from the project constructor, before
  workers. Its unsynchronized `done` guard prevents repeated registration;
  it is not a lock and is not reset by `ChemistryRegistry::ResetForTesting`.
- `ConstructReactionTable` runs from
  `G4DNAChemistryManager::InitializeMaster` (DNA utils source, lines 387–425).
  It builds the shared table after molecule construction.
- `ConstructProcess` registers per-thread processes and calls
  `buildBulkReactions` again. A bulk-list builder must return independent data
  without mutating shared state; it is not an exactly-once callback.
- `constructMolecules` can run during initial construction and again when
  `/chem/select` is applied on the master. Keep it idempotent.

`ChemistryRegistry` itself provides no synchronization. Register/select before
concurrent readers, and do not infer thread safety from selecting the same
name: `Select` still assigns its stored string. The previous recipe claimed
worker command replay called the same messenger harmlessly; that conclusion
was not established. `G4WorkerRunManager::DoWork` replays the command stack,
but `G4UImanager::ApplyCommand` resolves against the receiving thread's command
tree. Replay of a command is not proof of dispatch to the master-owned object.
No command-broadcast or synchronization behavior is changed in this pass.

## See also

- [`docs/adr/0002-named-chemistries.md`](../adr/0002-named-chemistries.md) —
  why reaction content is packaged as a named, selectable Chemistry and why
  the acid-base buffer lives per-Chemistry.
- [`docs/adr/0007-extra-molecules-per-chemistry.md`](../adr/0007-extra-molecules-per-chemistry.md)
  — the `constructMolecules` hook, worked example `Tonneau2025`'s `HO3`.
- [`create-a-macro-command.md`](create-a-macro-command.md) — the `/chem/select`
  messenger pattern (`ChemistrySelectMessenger`, a plain `G4UImessenger`
  attaching to the pre-existing `/chem/` directory) and the MT command-replay
  mechanism. Replay and actual dispatch are distinct, as described above.
- [`dnachem-min-macro-command-reference.md`](dnachem-min-macro-command-reference.md)
  — `/chem/select` and `/chem/list` alongside every other macro command.
