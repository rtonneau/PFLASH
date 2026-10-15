# Implementation Plan

**Session:** multiple-dna-chemistry
**Date:** 2026-09-28T12:40:10.900Z
**Estimated effort:** 1 day

## Strategy

Package the reaction content (reaction table plus acid-base buffer list) as a named **Chemistry** and pick one from the macro before `/run/initialize` with `/chem/select <name>`. Shared pieces stay shared: molecule set, dissociation channels, SBS time-step model, `DnaChemistryWorld`, `G4DNAScavengerMaterial`. Spec: `.work/sessions/2026-09-28__multiple-dna-chemistry/01-grill/resume.md`; decisions: `docs/adr/0002-named-chemistries.md`; vocabulary: `CONTEXT.md`.

Design lock, used by every ticket:

```cpp
// header/ChemistryTypes.hh (portable, std only)
namespace ChemistryTypes {
struct AcidBaseReaction { std::string partner; double rate;   // rate already dimensioned
                          std::vector<std::string> products; int reactionType = 0; };
struct AcidBaseEntry    { std::string molecule; std::vector<AcidBaseReaction> reactions; };
using AcidBaseList = std::vector<AcidBaseEntry>;               // may be empty
}
// header/ChemistryRegistry.hh (portable; forward-declares G4DNAMolecularReactionTable only)
namespace ChemistryRegistry {
struct Chemistry { std::string name;
                   void (*buildReactions)(G4DNAMolecularReactionTable*);
                   ChemistryTypes::AcidBaseList (*buildAcidBase)(); };
extern const char* const kDefaultName;                        // "PureWater"
bool Register(const Chemistry&, std::string& err);
bool Select(const std::string& name, std::string& err);
const Chemistry* Selected();                                  // picked one, else the default's entry
std::vector<std::string> Names();
void ResetForTesting();
}
```

The registry only stores function pointers, so it is unit-tested with dummy builders and no Geant4 kernel. Errors follow the `OutputDir` pattern: pure logic returns `bool` plus an error string, and the messenger raises the `G4Exception`.

Correction to the resume's success metrics: species yields are not reproducible run-to-run even with a fixed seed (`sim.cc` documents that chemistry stepping diverges). The deterministic `PureWater` regression check is therefore (a) the `/chem/reaction/dump` file compared before and after with lines sorted (the acid-base map is keyed by pointer, so line order can change), (b) identical `PhysicsInteractions.csv` and `EnergyDeposit.Txt` for the same seed (physics stage only), and (c) species yields checked for plausibility only.

## Tickets Overview

1. `01-chemistry-registry-core`: portable types and registry with a kernel-free unit test.
2. `02-purewater-acid-base-as-data`: capture the baseline first, then move the hard-coded acid-base list into `PureWaterReactions` as data. No behavior change.
3. `03-select-chemistry-command`: register `PureWater`, add `/chem/select` and `/chem/list`, make `DnaChemistryList` use the selected Chemistry.
4. `04-boscolochem-variant`: add `BoscoloChem` as an editable copy of `PureWater`, an example macro, and the empty-acid-base-list safety check.
5. `05-docs-and-final-verification`: update `CLAUDE.md`, README and source headers, then run the full test and smoke matrix.

## Sequencing Rationale

Ticket 01 adds files that nothing calls yet, so `sim` behaves as before. Ticket 02 changes structure but not behavior and is proven by the dump comparison against a baseline captured before the first edit. Ticket 03 is the first user-visible change and can only be verified once the data-driven acid-base path from 02 exists. Ticket 04 needs the registry and command from 03. Ticket 05 documents the final state and runs the whole matrix once.

## Risks & Mitigation

- Empty acid-base list might crash or warn because `G4DNAScavengerMaterial` is installed with no scavenger processes. Ticket 04 tests it explicitly by temporarily returning `{}`; if it fails, read the trace, then gate the installation in `DnaChemistryList::ConstructProcess` and record it in ADR 0002.
- Reaction-dump line order is not stable. Compare with `sort` on both sides.
- Brace-initialization of nested `std::vector` aggregates can pick the wrong constructor. If the compiler complains in ticket 02, spell the element types out (`ChemistryTypes::AcidBaseReaction{...}`).
- `/chem/` already exists (created by `G4DNAChemistryManager`). Ticket 03 uses a plain `G4UImessenger` with two commands rather than `G4GenericMessenger("/chem/")`, which would create a duplicate directory. Verify in the run with `/control/manual /chem/` that both commands appear.
- MSVC Debug CRT hangs on a failing `assert()`. The new test starts `main()` with the CRT report-mode block from `.claude/geant4-instructions.md` section 5.

## Assumptions

- All target chemistries fit the shared molecule set and dissociation channels (`G4ChemDissociationChannels_option1`). A chemistry needing new species is a separate change.
- The registry is read-only after startup (`RegisterBuiltIn` runs once from the `DnaChemistryList` constructor on the master thread), so worker threads can read it safely.
- `/chem/list` prints with plain `G4cout` because the user issues it explicitly (not `DnaLogger`-gated).
- `BoscoloChem` physics is not validated here. The user edits its values.
- The run-environment companion file is a separate session (`/gps start run-environment-record`).

## Token Usage

- **Input:** 24
- **Output:** 44094
- **Cache read:** 1391169
- **Cache creation:** 69220
- **Total:** 1504507
