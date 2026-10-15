# Ticket 04 Implementation

**Status:** ✅ Done

## Commits

- feat: Fig. 3 comparison tool and digitized reference (ticket 04) -- SHA in git log (commit message references ticket 04)

## Local Test Result

```
python analysis/compare_boscolo_fig3.py runs/o2_0 runs/o2_21 --out t04.png ; exit=1
FAIL  H(0%) non-increasing  2 rise(s) > 0.02, first at 1e-09 s (0.737 -> 0.804)
FAIL  O2m(21%) saturated    G(1e-07s)=1.341 G(1us)=3.136 rel.change=0.573
FAIL  HO2(21%) saturated    G(1e-07s)=0.346 G(1us)=0.878 rel.change=0.605
FAIL  HO2(0%) ~ 0           G(1us)=0.062
FAIL  O2m(21%) +-10%        G(1us)=3.136 vs 2.24 (+40.0%)
FAIL  HO2(21%) +-10%        G(1us)=0.878 vs 0.66 (+33.0%)
PASS  e_aq(0%) +-10%        G(1us)=2.034 vs 2.25 (-9.6%)
FAIL  H(0%) +-10%           G(1us)=0.852 vs 0.55 (+54.9%)
```
Required FAILs (H non-increasing, HO2(21%) saturated, O2m(21%) +-10%) all present; exit 1; PNG written. With only one dump dir the missing-level checks print SKIP (verified for o2_21 alone). Reference spot values (CSV): e_aq(0,1e-6)=2.269, O2m(21,1e-6)=2.241, HO2(21,1e-6)=0.661, H(0,1e-6)=0.560, H(all,1e-12)=0.81-0.83, e_aq(all,1e-12)=4.49-4.50.

## Review Notes

- Reference was not eyeballed: Fig. 3 is vector art in the PDF, so curve paths and axis ticks were read with PyMuPDF `get_drawings()` (colour per pO2 level, y calibrated on left-spine ticks, x on 1e-12..1e-6 frame edges). The HO2 panel contains a duplicate offset copy of its curves, filtered by chain continuity. Digitization scripts live in the scratch dir (04-extract.py, 04-digitize.py), not in the repo.
- Hardest to read: e_aq/H curves at early times (overlap within line width); pO2=0 O2m/HO2 curves are not drawn separately (stored as 0). e_aq(0%,1e-6)=2.269 and H(0%,1e-6)=0.560 are within the 0.02 tolerance of the ticket spots (2.25, 0.55; paper text says 0.56 for H).
- Sim output times are decades (1 ps ... 100 ns, 999.999 ns), so there is no 0.5 us row; "saturated" compares 1 us with the last output time <= 0.55 us (100 ns here). Documented in README.
- pO2 snapped to nearest reference level; 0.000273 M O2 -> 21 %.

## Time Spent

~1 hour

## Blockers / Challenges

None

## Token Usage

- **Input:** 72
- **Output:** 4103
- **Cache read:** 2814240
- **Cache creation:** 101425
- **Total:** 2919840
