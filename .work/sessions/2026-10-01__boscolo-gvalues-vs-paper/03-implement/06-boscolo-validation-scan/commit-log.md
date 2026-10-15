# Ticket 06 Implementation

**Status:** In Progress

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- None (BLOCKED; nothing committed, macro/CLAUDE.md edits reverted)

## Local Test Result

```
PASS  H(0%) non-increasing  0 rise(s) > 0.02
PASS  O2m(21%) saturated    G(5e-07s)=3.086 G(1us)=3.211 rel.change=0.039
FAIL  HO2(21%) saturated    G(5e-07s)=0.653 G(1us)=0.736 rel.change=0.112
PASS  HO2(0%) ~ 0           G(1us)=0.000
FAIL  O2m(21%) +-10%        G(1us)=3.211 vs 2.24 (+43.3%)
FAIL  HO2(21%) +-10%        G(1us)=0.736 vs 0.66 (+11.4%)
FAIL  e_aq(0%) +-10%        G(1us)=2.538 vs 2.25 (+12.8%)
FAIL  H(0%) +-10%           G(1us)=0.694 vs 0.55 (+26.1%)
Burst check (21%, H3O+ + O2- -> HO2): 100-300 ns = 6, 300 ns-1 us = 119, limit 63 -> FAIL
```

## Review Notes

Full table, 21% per-bin reaction counts and wall times are in .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/validation.md. In the 300 ns-1 us bin, several reactions jump 6-20x at once (H3O+ + O2-, OH + O2-, OH + OH, H3O+ + OH-), which looks like an end-of-chemistry burst.

## Time Spent

~0.9 hours (mostly sim wall time; longest level 21% = 2437 s)

## Blockers / Challenges

BLOCKED: shape check `HO2(21%) saturated` fails (rel. change 0.112 > 0.05), and the no-burst criterion fails (119 vs a limit of 63). Per the ticket, nothing was changed: the beam_boscolo.in and CLAUDE.md edits were reverted.

## Token Usage

- **Input:** <!-- gps:fill input tokens -->
- **Output:** <!-- gps:fill output tokens -->
- **Cache read:** <!-- gps:fill cache read tokens -->
- **Cache creation:** <!-- gps:fill cache creation tokens -->
- **Total:** <!-- gps:fill total tokens -->
