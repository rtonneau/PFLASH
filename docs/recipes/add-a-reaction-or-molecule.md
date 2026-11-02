# Add a reaction or molecule

## What this is

How to extend an *existing* Chemistry's reaction table, bulk-reaction list, or
molecule set — adding one reaction or one molecule inside a `catalog/`
builder that is already registered. This is a smaller task than
[`add-a-new-chemistry.md`](add-a-new-chemistry.md), which adds a whole new
named Chemistry; here the Chemistry already exists and only its content
grows. Reaction/molecule data is declared as plain data inside a builder
function (`src/chemistry/catalog/*.cc`), never through the macro layer
(`/chem/reaction/UI` resets the shared reaction table in
`G4ReactionTableMessenger::SetNewValue`; macro-built reactions are not this
project's source of reaction content).

## Generic pattern

In Geant4-DNA, the chemical stage is driven by:

- a `G4DNAMolecularReactionTable` populated with `G4DNAMolecularReactionData`
  objects (one reactant pair, a rate constant, and zero or more products via
  `AddProduct()`), looked up by `G4MolecularConfiguration*` pointers obtained
  from `G4MoleculeTable::Instance()->GetConfiguration(name)`;
- optionally, bulk/background reactions against an acid-base buffer species
  or a dissolved scavenger; buffer concentrations and consumable scavenger
  populations have different lifecycles. These are registered as a
  `G4DNAScavengerProcess` per molecule for the particle-based stage and also
  added to the reaction table for the mesoscopic stage;
- a reaction's **type** (0 = fully diffusion-controlled, 1 = partially
  diffusion-controlled with an activation step sampled at the van der Waals
  radius) set once via `G4DNAMolecularReactionData::SetReactionType(1)` — a
  call that cannot be undone, so it must be gated rather than applied
  unconditionally when a chemistry mode (SBS) requires every reaction to stay
  type 0;
- for a molecule outside the stock set, a `G4MoleculeDefinition` plus its
  default `G4MolecularConfiguration`, created once and looked up afterwards
  (not reconstructed every time something references it by name).

## dnachem-min reference implementation

Verified against the current `src/chemistry/catalog/PureWaterReactions.cc`.

### Adding a reaction to the (non-bulk) reaction table

`PureWaterReactions::BuildPureWaterReactions` (`PureWaterReactions.cc`)
builds two local lambdas around the species lookups:

```cpp
auto add = [reactionTable](MolConf reactant1, MolConf reactant2, G4double rate_M_inv_s_inv,
                           std::initializer_list<MolConf> products) {
  auto* reactionData =
    new G4DNAMolecularReactionData(rate_M_inv_s_inv * (1e-3 * m3 / (mole * s)),
                                 reactant1, reactant2);
  for (auto* moleculeConfiguration : products) {
    reactionData->AddProduct(moleculeConfiguration);
  }
  reactionTable->SetReaction(reactionData);
  return reactionData;
};
auto partial = [&add](MolConf reactant1, MolConf reactant2, G4double rate_M_inv_s_inv,
                      std::initializer_list<MolConf> products) {
  auto* reactionData = add(reactant1, reactant2, rate_M_inv_s_inv, products);
  if (ChemistryTypes::PartialReactionsEnabled()) {
    reactionData->SetReactionType(1);
  }
};
```

(`PureWaterReactions.cc`). Species are resolved once, up front, through
a local `Conf(name)` helper
(`G4MoleculeTable::Instance()->GetConfiguration(name)`, fatal `G4Exception`
on an unknown name, `PureWaterReactions.cc`), then every reaction is one
call to `add(...)` (type 0, the project's default) or `partial(...)` (type 1
unless `ChemistryTypes::PartialReactionsEnabled()` reads `false`), e.g.:

```cpp
add(H, H, 1.2e10, {H2});                 // fully diffusion-controlled
partial(e_aq, OH, 2.95e10, {OHm});        // partially diffusion-controlled
```

(`PureWaterReactions.cc`). To add a reaction to an existing
Chemistry:

1. Open the Chemistry's `BuildXxxReactions` function in its `catalog/*.cc`
   file.
2. Resolve any new species via `Conf(name)` (or the equivalent local helper)
   near the top, alongside the existing lookups — never inline, so a missing
   species fails fast with a clear name rather than a null-pointer crash
   later.
3. Add one `add(...)` or `partial(...)` call (or the local equivalent) with
   the rate constant converted to Geant4 internal units — this file's
   convention is `rate_M_inv_s_inv * (1e-3 * m3 / (mole * s))` for a bimolecular
   M⁻¹ s⁻¹ rate (`PureWaterReactions.cc`) — and the product list.
4. Use `partial(...)` (type 1) only for a reaction that is genuinely
   partially diffusion-controlled per the project's classification (see the
   `Tonneau2025Reactions.hh` header comment's per-reaction table for a worked
   example of that classification against `G4EmDNAChemistry_option3`'s
   own Type I–IV scheme); use `add(...)` (type 0, left unset) otherwise. Call
   `SetReactionType(1)` through the gated helper, never directly — see
   "Respecting `SetReactionType`/`SetPartialReactionsEnabled`" below.

### Adding a bulk reaction

`PureWaterReactions::BuildPureWaterBulkReactions()` returns a
`ChemistryTypes::BulkReactionList` as plain data — no Geant4 calls at all
(`PureWaterReactions.cc`). The type is declared in
`header/chemistry/ChemistryTypes.hh`:

```cpp
struct BulkReaction { std::string partner; double rate;
                       std::vector<std::string> products; int reactionType = 0; };
struct BulkReactionEntry { std::string molecule; std::vector<BulkReaction> reactions; };
using BulkReactionList = std::vector<BulkReactionEntry>;
```

One entry per tracked molecule, each with a list of reactions against a bulk
partner — an acid-base species (`"H3Op(B)"`, `"OHm(B)"`, `"H2O"`) or a
scavenger (`"O2"`):

```cpp
{"e_aq",
 {{"H3Op(B)", 2.11e10 * secondOrderRateUnit, {"H"}, 0},
  {"H2O", 1.57e1 * waterConcentration_M / s, {"H", "OHm(B)"}, 0},
  {"O2", 1.74e10 * secondOrderRateUnit, {"O2m"}, 0}}},
```

The constants `secondOrderRateUnit` and `waterConcentration_M` are defined
above these entries in `PureWaterReactions.cc`. To add a bulk reaction: add a
`BulkReaction` to an existing molecule's vector, or a new
`BulkReactionEntry` for a molecule with no bulk reactions yet. The driver
(`DnaChemistryList`) turns each entry into one `G4DNAScavengerProcess` per
molecule for the particle-based stage, and also adds every bulk reaction to
the reaction table for the mesoscopic stage — a pair already there (e.g.
`e_aq` + tracked `O2`) must carry the same rate and products, since both
share that one table entry (see `PureWaterReactions.hh`, and
`Tonneau2025Reactions.hh`'s "Routing" section for several worked examples of
a pair that is deliberately in both places).

### Respecting `SetReactionType`/`SetPartialReactionsEnabled`

`ChemistryTypes::PartialReactionsEnabled()` (`ChemistryTypes.hh`) is a
process-wide switch the driver sets once, before any Chemistry builds its
tables: `true` under IRT_syn (the default), `false` under SBS. A reaction
that would be partially diffusion-controlled must check it before calling
`SetReactionType(1)`, as `partial()` above does — never call
`SetReactionType(1)` unconditionally, because the call cannot be undone and
would silently break SBS mode (where every reaction must stay type 0, fully
diffusion-controlled, matching chem1–chem6 and the SBS reference). A bulk
reaction's `reactionType` field (the acid-base equilibria use 6/7/8,
`ChemistryTypes.hh`) is a different axis — those are the
`G4ChemEquilibrium` types already baked into the data, not the IRT activation
type, and are not gated by `PartialReactionsEnabled()`.

### Adding a molecule: the "Extra molecule" pattern

A molecule outside the shared stock set (`G4ChemDissociationChannels_option1`)
can be added, but only as a per-Chemistry **Extra molecule**, not by adding
it to the shared set — see
[`docs/adr/0007-extra-molecules-per-chemistry.md`](../adr/0007-extra-molecules-per-chemistry.md)
for why (molecule definitions live in a process-wide `G4MoleculeTable`; adding
one unconditionally would perturb every other Chemistry's output). The
worked example is HO3 under `Tonneau2025`,
`src/chemistry/catalog/Tonneau2025Reactions.cc`'s
`ConstructTonneau2025Molecules()`:

```cpp
void Tonneau2025Reactions::ConstructTonneau2025Molecules()
{
  auto* molTable = G4MoleculeTable::Instance();
  if (molTable->GetConfiguration(kHO3, false) != nullptr) {
    return;  // already created
  }
  G4MoleculeDefinition* definition = molTable->GetMoleculeDefinition(kHO3, false);
  if (definition == nullptr) {
    definition = new G4MoleculeDefinition(kHO3, kHO3Mass, kHO3Diffusion, 0, 0, kHO3Radius, 4);
    definition->SetLevelOccupation(0);
    definition->SetFormatedName("HO_{3}");
  }
  auto* configuration = G4MolecularConfiguration::GetOrCreateMolecularConfiguration(definition);
  configuration->SetDiffusionCoefficient(kHO3Diffusion);
  configuration->SetVanDerVaalsRadius(kHO3Radius);
}
```

(`Tonneau2025Reactions.cc`). Two things this function is careful
about, both required of any new Extra molecule:

- **Idempotence.** `new G4MoleculeDefinition(...)` is *not* idempotent (a
  second call for the same name is a distinct error from re-registering a
  configuration), so the function checks `GetConfiguration`/
  `GetMoleculeDefinition` first and returns early, or reuses the existing
  definition, rather than constructing unconditionally. This matters because
  a repeated selection can invoke the hook again; exactly two calls are not guaranteed.
- **One default configuration per definition.** The configuration created is
  the definition's *default* one (`GetOrCreateMolecularConfiguration`, whose
  user ID is the definition's name) — a separate
  `CreateConfiguration("HO3", ...)` call would not be found by
  `G4DNAChemistryManager`'s later lookup and would collide (fatal
  `CONF_ALREADY_RECORDED`), per the comment at
  `Tonneau2025Reactions.cc`.

Wiring: the function is registered as the Chemistry's `constructMolecules`
hook — a nullable third function pointer on `ChemistryRegistry::Chemistry`
(`header/chemistry/ChemistryRegistry.hh`) — in
`src/chemistry/BuiltInChemistries.cc`:

```cpp
add({"Tonneau2025", &Tonneau2025Reactions::BuildTonneau2025Reactions,
     &Tonneau2025Reactions::BuildTonneau2025BulkReactions,
     &Tonneau2025Reactions::ConstructTonneau2025Molecules});
```

`DnaChemistryList::ConstructMolecule` calls the selected Chemistry's hook,
when non-null, after the shared molecule set and the `H2O` configuration
exist. Because `ConstructMolecule` runs before any macro command can select a
Chemistry, `/chem/select` (`ChemistrySelectMessenger`) also calls the hook —
hence the idempotence requirement above. Initially selected `PureWater` has
no Extra-molecule hook, so the first Tonneau2025 hook call normally comes from
`/chem/select`; selecting the same Chemistry again can repeat it. See ADR 0007 for the full rationale, including why a
per-Chemistry hook was chosen over adding the molecule to the shared set.

A Chemistry without an extra molecule (`PureWater`, `BoscoloChem`) simply
leaves this third function pointer null — nothing else changes.

## Serial vs MT

`PhysicsList` owns one shared `DnaChemistryList`, but its constructor and its
Geant4 callbacks have different lifecycles:

- `ConstructReactionTable` is called by
  `G4DNAChemistryManager::InitializeMaster` to build the shared reaction table.
- `ConstructProcess` registers this thread's processes and invokes the bulk-list
  builder. Return independent list data; do not mutate shared builder state.
- The Extra-molecule hook runs during molecule setup when selected, and from
  master-side `/chem/select`. Guard repeated creation.

See [`add-a-new-chemistry.md`](add-a-new-chemistry.md#serial-vs-mt) for the
Geant4 `.cc` source trace. Shared object ownership does not imply that all
builder functions run exactly once or only on the master. Finish shared
registry/selection changes before concurrent readers.

## See also

- [`add-a-new-chemistry.md`](add-a-new-chemistry.md) — adding a whole new
  named Chemistry (this recipe assumes the Chemistry already exists); its
  `## Serial vs MT` section is the source for the threading claim above.
- [`docs/adr/0002-named-chemistries.md`](../adr/0002-named-chemistries.md) —
  why reaction content is packaged as a named, selectable Chemistry.
- [`docs/adr/0007-extra-molecules-per-chemistry.md`](../adr/0007-extra-molecules-per-chemistry.md)
  — the `constructMolecules` hook and why HO3 is per-Chemistry, not shared.
- `src/chemistry/catalog/PureWaterReactions.cc` /
  `header/chemistry/catalog/PureWaterReactions.hh` — the reference builder
  for ordinary reactions and bulk reactions.
- `src/chemistry/catalog/Tonneau2025Reactions.cc` /
  `header/chemistry/catalog/Tonneau2025Reactions.hh` /
  `src/chemistry/catalog/Tonneau2025Table.cc` — the worked "Extra molecule"
  example (HO3) and a second, larger worked example of reaction-table and
  bulk-reaction routing (73 rows, several reactions shared between table and
  bulk).
- `header/chemistry/ChemistryTypes.hh` — `BulkReaction`/`BulkReactionEntry`/
  `BulkReactionList` and `PartialReactionsEnabled`/`SetPartialReactionsEnabled`.
