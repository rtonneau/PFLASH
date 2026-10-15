# Add a reaction or molecule

## What this is

How to extend an *existing* Chemistry's reaction table, bulk-reaction list, or
molecule set — adding one reaction or one molecule inside a `catalog/`
builder that is already registered. This is a smaller task than
[`add-a-new-chemistry.md`](add-a-new-chemistry.md), which adds a whole new
named Chemistry; here the Chemistry already exists and only its content
grows. Reaction/molecule data is declared as plain data inside a builder
function (`src/chemistry/catalog/*.cc`), never through the macro layer
(`/chem/species` / `/chem/reaction/add` are reset by `/chem/reaction/UI` and
are not how this project's reactions are defined).

## Generic pattern

In Geant4-DNA, the chemical stage is driven by:

- a `G4DNAMolecularReactionTable` populated with `G4DNAMolecularReactionData`
  objects (one reactant pair, a rate constant, and zero or more products via
  `AddProduct()`), looked up by `G4MolecularConfiguration*` pointers obtained
  from `G4MoleculeTable::Instance()->GetConfiguration(name)`;
- optionally, bulk/background reactions against a fixed-concentration partner
  (an acid-base buffer species or a dissolved scavenger), registered as a
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

`PureWaterReactions::BuildPureWaterReactions` (`PureWaterReactions.cc:40-143`)
builds two local lambdas around the species lookups:

```cpp
auto add = [reactionTable](MolConf a, MolConf b, G4double k,
                           std::initializer_list<MolConf> products) {
  auto* rd = new G4DNAMolecularReactionData(k * (1e-3 * m3 / (mole * s)), a, b);
  for (auto* p : products) {
    rd->AddProduct(p);
  }
  reactionTable->SetReaction(rd);
  return rd;
};
auto partial = [&add](MolConf a, MolConf b, G4double k,
                      std::initializer_list<MolConf> products) {
  auto* rd = add(a, b, k, products);
  if (ChemistryTypes::PartialReactionsEnabled()) {
    rd->SetReactionType(1);
  }
};
```

(`PureWaterReactions.cc:47-62`). Species are resolved once, up front, through
a local `Conf(name)` helper
(`G4MoleculeTable::Instance()->GetConfiguration(name)`, fatal `G4Exception`
on an unknown name, `PureWaterReactions.cc:27-35`), then every reaction is one
call to `add(...)` (type 0, the project's default) or `partial(...)` (type 1
unless `ChemistryTypes::PartialReactionsEnabled()` reads `false`), e.g.:

```cpp
add(H, H, 1.2e10, {H2});                 // fully diffusion-controlled
partial(e_aq, OH, 2.95e10, {OHm});        // partially diffusion-controlled
```

(`PureWaterReactions.cc:84`, `89`). To add a reaction to an existing
Chemistry:

1. Open the Chemistry's `BuildXxxReactions` function in its `catalog/*.cc`
   file.
2. Resolve any new species via `Conf(name)` (or the equivalent local helper)
   near the top, alongside the existing lookups — never inline, so a missing
   species fails fast with a clear name rather than a null-pointer crash
   later.
3. Add one `add(...)` or `partial(...)` call (or the local equivalent) with
   the rate constant converted to Geant4 internal units — this file's
   convention is `k * (1e-3 * m3 / (mole * s))` for a bimolecular
   M⁻¹ s⁻¹ rate (`PureWaterReactions.cc:49`) — and the product list.
4. Use `partial(...)` (type 1) only for a reaction that is genuinely
   partially diffusion-controlled per the project's classification (see the
   `Tonneau2025Reactions.hh` header comment's per-reaction table for a worked
   example of that classification against `G4EmDNAMolecularReactionTable`'s
   own Type I–IV scheme); use `add(...)` (type 0, left unset) otherwise. Call
   `SetReactionType(1)` through the gated helper, never directly — see
   "Respecting `SetReactionType`/`SetPartialReactionsEnabled`" below.

### Adding a bulk reaction

`PureWaterReactions::BuildPureWaterBulkReactions()` returns a
`ChemistryTypes::BulkReactionList` as plain data — no Geant4 calls at all
(`PureWaterReactions.cc:147-179`). The type is declared in
`header/chemistry/ChemistryTypes.hh:16-33`:

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
 {{"H3Op(B)", 2.11e10 * M, {"H"}, 0},
  {"H2O", 1.57e1 * cW / s, {"H", "OHm(B)"}, 0},
  {"O2", 1.74e10 * M, {"O2m"}, 0}}},
```

(`PureWaterReactions.cc:161-164`, `M`/`cW` defined just above at
`PureWaterReactions.cc:149-150`). To add a bulk reaction: add a
`BulkReaction` to an existing molecule's vector, or a new
`BulkReactionEntry` for a molecule with no bulk reactions yet. The driver
(`DnaChemistryList`) turns each entry into one `G4DNAScavengerProcess` per
molecule for the particle-based stage, and also adds every bulk reaction to
the reaction table for the mesoscopic stage — a pair already there (e.g.
`e_aq` + tracked `O2`) must carry the same rate and products, since both
share that one table entry (see `PureWaterReactions.hh:35-38`, and
`Tonneau2025Reactions.hh`'s "Routing" section for several worked examples of
a pair that is deliberately in both places).

### Respecting `SetReactionType`/`SetPartialReactionsEnabled`

`ChemistryTypes::PartialReactionsEnabled()` (`ChemistryTypes.hh:40-55`) is a
process-wide switch the driver sets once, before any Chemistry builds its
tables: `true` under IRT_syn (the default), `false` under SBS. A reaction
that would be partially diffusion-controlled must check it before calling
`SetReactionType(1)`, as `partial()` above does — never call
`SetReactionType(1)` unconditionally, because the call cannot be undone and
would silently break SBS mode (where every reaction must stay type 0, fully
diffusion-controlled, matching chem1–chem6 and the SBS reference). A bulk
reaction's `reactionType` field (the acid-base equilibria use 6/7/8,
`ChemistryTypes.hh:21`) is a different axis — those are the
`G4ChemEquilibrium` types already baked into the data, not the IRT activation
type, and are not gated by `PartialReactionsEnabled()`.

### Adding a molecule: the "Extra molecule" pattern

A molecule outside the shared stock set (`G4ChemDissociationChannels_option1`)
can be added, but only as a per-Chemistry **extra molecule**, not by adding
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

(`Tonneau2025Reactions.cc:304-330`). Two things this function is careful
about, both required of any new extra molecule:

- **Idempotence.** `new G4MoleculeDefinition(...)` is *not* idempotent (a
  second call for the same name is a distinct error from re-registering a
  configuration), so the function checks `GetConfiguration`/
  `GetMoleculeDefinition` first and returns early, or reuses the existing
  definition, rather than constructing unconditionally. This matters because
  the hook below runs more than once per process in the ordinary case.
- **One default configuration per definition.** The configuration created is
  the definition's *default* one (`GetOrCreateMolecularConfiguration`, whose
  user ID is the definition's name) — a separate
  `CreateConfiguration("HO3", ...)` call would not be found by
  `G4DNAChemistryManager`'s later lookup and would collide (fatal
  `CONF_ALREADY_RECORDED`), per the comment at
  `Tonneau2025Reactions.cc:321-326`.

Wiring: the function is registered as the Chemistry's `constructMolecules`
hook — a nullable third function pointer on `ChemistryRegistry::Chemistry`
(`header/chemistry/ChemistryRegistry.hh:24-29`) — in
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
hence the idempotence requirement above: the hook genuinely does run twice
in the normal case (once from `ConstructMolecule`, once from `/chem/select`),
not just defensively. See ADR 0007 for the full rationale, including why a
per-Chemistry hook was chosen over adding the molecule to the shared set.

A Chemistry without an extra molecule (`PureWater`, `BoscoloChem`) simply
leaves this third function pointer null — nothing else changes.

## Serial vs MT

A reaction or molecule added this way lives inside the same construction
path as the Chemistry itself — `BuildXxxReactions`, `BuildXxxBulkReactions`
and `constructMolecules` are all called from `DnaChemistryList`'s
construction (plus `constructMolecules` again from `/chem/select`), not from
any per-worker-thread code path. Ticket 8's
[`add-a-new-chemistry.md`](add-a-new-chemistry.md) `## Serial vs MT` section
traces this precisely against the Geant4 kernel source: `DnaChemistryList`
is constructed exactly once per process in both Serial and MT (the kernel
hands every worker the *same* `G4VUserPhysicsList` pointer rather than
rebuilding it — see `G4MTRunManagerKernel::StartThread`/
`G4TaskRunManagerKernel::StartThread`), so none of these builder functions
need to be thread-safe against concurrent calls from different workers; the
only documented hazard is re-entrancy within one process, which is why
`constructMolecules` must be idempotent. Follow that recipe's `## Serial vs
MT` section rather than re-deriving the proof here — a reaction/molecule
addition does not change the construction path's threading story at all, it
only adds content inside it.

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
