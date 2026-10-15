# Session Summary: review how Reaction Label are attributed and computed. For the moment each call to TimeStepAction::UserReactionAction reconstruc reaction's label. Wouldn't it be more efficient to compute all reaction labels once at simulation init. (before start) and reuse it when needed? Also in "const auto *reactionData = G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB);", it means that there is only one entry for molA and molB and there cannot be two different reactions with same reactants?

**Session ID:** 2026-09-23__review-how-reaction-label-are-attributed-and-computed-for-the
**Created:** 2026-09-23T12:28:58.876Z
**Finished:** 2026-09-23T12:53:58.672Z
**Status:** Complete (bounded: no plan or tickets)

## Grill

- Resume: [01-grill/resume.md](01-grill/resume.md)

## Next

Start a new feature with /gps start <next-feature>
