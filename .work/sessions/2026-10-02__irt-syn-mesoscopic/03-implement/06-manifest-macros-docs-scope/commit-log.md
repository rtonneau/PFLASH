# Ticket 06 Implementation

**Status:** ✅ Done

## Commits

- `c16b72d` docs+chore: manifest model keys, macros without SBS, scope change (ticket 06)

## Local Test Result

```
Inline re-run (after the inline fixes), 06-smoke.in --threads 2 (10 keV x 2, end 1 ms):
  build exit 0, smoke exit 0, 0 EEEE
  Manifest.json: chemistryModel "IRT_syn+mesoscopic", handOverTime_ns 5, voxelSize_nm 6.25,
                 mesoPixels 65536, mesoTimesPerDecade 10, chemistryEndTime_ns 1000000
  macro grep "TimeStepModel SBS" (macro/*.in, *.ps1): none
```

## Review Notes

- Implemented by a subagent on **sonnet**. The auto-mode classifier denied a `git rm` of `beam_02.in`, so the subagent repurposed it as the Meesungnoen2002 variant, which the ticket allows.
- **Inline follow-up fixes:**
  - **Reaction-binning macros:** `reaction_counter.in` and `reactions.in` bin reactions with fixed widths up to the end time. With the new 1 s default that would be about 10¹⁰ and 10⁹ bins. Both now set `/scheduler/endTime 1 us` after `/run/initialize`, and their comments explain why (reactions are counted up to the hand-over only).
  - **README:** the reaction-counter example's macro listing and text are updated to match.
  - **CLAUDE.md:** the scope sentence now says the project scope has changed once (ADR 0006), instead of keeping the blanket ban on dedicated chemistry models.
- **Edits beyond the ticket's file list, all accepted:** README, `.claude/.claude-project.json` (smoke note, `SpeciesMeso.*` added to the outputs), `reactions.in`, `reaction_counter.in`, `test_run.ps1`.
- The `WrongResolution` benign warning is documented in the `TimeStepAction` key-file entry.

## Time Spent

~0.3 hours

## Blockers / Challenges

None.

## Token Usage

- **Input:** 54
- **Output:** 6605
- **Cache read:** 6514068
- **Cache creation:** 113894
- **Total:** 6634621
