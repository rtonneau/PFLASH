# Session: review how Reaction Label are attributed and computed. For the moment each call to TimeStepAction::UserReactionAction reconstruc reaction's label. Wouldn't it be more efficient to compute all reaction labels once at simulation init. (before start) and reuse it when needed? Also in "const auto *reactionData = G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB);", it means that there is only one entry for molA and molB and there cannot be two different reactions with same reactants?

**Date:** 2026-09-23T12:28:58.876Z
**Status:** Grill phase complete

## Problem Statement

`TimeStepAction::UserReactionAction` (`src/TimeStepAction.cc:103-112`) rebuilds
the reaction label from scratch on every single bimolecular reaction firing:
it allocates a `std::vector<G4String>` of product names, then calls
`ReactionTableDump::FormatReactionLabel`, which does an `ostringstream` plus
several string concatenations. This happens once per fired reaction across
the whole run (potentially very many times), even though the label for a
given reaction never changes once the reaction table is built.

## Context & Constraints

- Each `G4DNAMolecularReactionData` carries a stable, contiguous 1-based
  integer ID (`GetReactionID()`), assigned once in
  `G4DNAMolecularReactionTable::SetReaction()` (Geant4 v11.4.1 source,
  `source/processes/electromagnetic/dna/utils/src/G4DNAMolecularReactionTable.cc:395-413`)
  when the table is built. That ID space covers exactly the bimolecular
  reactions registered via `SetReaction()` — acid-base/scavenger reactions
  use a separate mechanism (`G4DNAScavengerProcess`) and never consume an ID
  from this table, consistent with the existing note that they "never reach
  that hook" for counting purposes.
- Confirmed from the same Geant4 source that
  `G4DNAMolecularReactionTable::fReactionData` is
  `std::map<Reactant*, std::map<Reactant*, Data*>>`, and `SetReaction()`
  does a plain map assignment (`fReactionData[r1][r2] = pReactionData;`).
  So there is at most one reaction entry per unordered (molA, molB) pair; a
  second `SetReaction()` call for the same pair would silently overwrite the
  first with no duplicate warning. This is a latent risk worth knowing about
  in `PureWaterReactions::BuildPureWaterReactions`, but auditing for actual
  duplicate registrations is out of scope for this change.
- `ReactionTableDump` (`src/ReactionTableDump.cc`) already owns
  `FormatReactionLabel` and already walks
  `G4DNAMolecularReactionTable::GetReactionTable()->GetVectorOfReactionData()`
  once, for the existing `/chem/reaction/dump` feature (`WriteBimolecular`).
  That dump only runs if the macro sets `/chem/reaction/dump`, so it can't be
  relied on to have already populated a cache before `UserReactionAction`
  needs one.
- `ReactionCounter`'s own `reactionId` (in `Reactions_nt_reactions.csv` /
  `ReactionsMetadata.csv`, via `BuildReactionIdMap`) is a separate,
  self-assigned alphabetical-order ID scheme, decoupled from Geant4's
  `GetReactionID()`. This change does not touch that scheme or the CSV/txt
  output formats — `ReactionCounter::Record` still receives the same string
  label as before, so output is expected to be byte-identical.
- Per `CLAUDE.md`'s testing scope note ("Geant4-object code ... is verified
  by the smoke run"), this wraps a live `G4DNAMolecularReactionTable`
  singleton, so no new CTest unit test is planned — verification is a
  before/after smoke-run diff.

## Success Metrics

- `TimeStepAction::UserReactionAction` no longer allocates a
  `std::vector<G4String>` or builds an `ostringstream` per reaction firing;
  it does an O(1) lookup by `reactionData->GetReactionID()` instead.
- `./sim reaction_counter.in` run before and after the change produces
  byte-identical `Reactions.Txt`, `Reactions_nt_reactions.csv`, and
  `ReactionsMetadata.csv`.

## Architecture & Approach

Extend `ReactionTableDump` (chosen over a new `ReactionLabelCache` class —
keeps "translate reaction-table data to a label" in one place, since that
namespace already owns `FormatReactionLabel` and already walks
`GetVectorOfReactionData()` for the dump feature):

- **`header/ReactionTableDump.hh`**: add one declaration,
  `const G4String& LabelFor(G4int reactionID);`
- **`src/ReactionTableDump.cc`**:
  - Add a private `BuildLabelCache()` that walks
    `G4DNAMolecularReactionTable::GetReactionTable()->GetVectorOfReactionData()`
    once and fills a `std::vector<G4String>` sized by `GetNReactions()`,
    indexed by `reactionID - 1`, using the existing `FormatReactionLabel`.
  - `LabelFor(id)` triggers the build on first call via a function-local
    `static` guarded by `std::call_once` (thread-safe against concurrent
    first-firing on different worker threads; read-only after the one-time
    build, matching the existing pattern where the reaction table itself is
    built master-only and read without locking by workers afterward).
- **`src/TimeStepAction.cc`** (`UserReactionAction`, lines 103-114): replace
  the per-call product-name loop + `FormatReactionLabel(...)` call with:
  ```cpp
  const G4String& label = ReactionTableDump::LabelFor(reactionData->GetReactionID());
  fReactionCounter.Record(label, G4Scheduler::Instance()->GetGlobalTime());
  ```
  The existing `reactionData == nullptr` guard (lines 96-101) is unchanged.

## Assumptions & Trade-offs

- Lazy (first-call) build via `std::call_once` was chosen over an eager
  build triggered explicitly from `DnaChemistryList::ConstructReactionTable()`,
  to avoid adding a new call site / cross-file ordering dependency; the
  trade-off is the very first reaction firing in a run pays the one-time
  build cost instead of it happening during setup, which is negligible next
  to the per-run simulation cost.
- `ReactionCounter`'s alphabetical `reactionId` scheme is intentionally left
  as-is — unifying it with Geant4's `GetReactionID()` would change the
  meaning of existing CSV output and is a separate, bigger change than what
  was asked here (YAGNI).

## Open Questions

None outstanding — design approved as presented.

## Notes

Not filing a duplicate-reactant-pair audit of
`PureWaterReactions::BuildPureWaterReactions` as part of this change; flagged
as a separate possible follow-up if desired.

## Token Usage

- **Input:** 30
- **Output:** 15012
- **Cache read:** 1724335
- **Cache creation:** 36282
- **Total:** 1775659
