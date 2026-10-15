# Ticket 02 Implementation

**Status:** ✅ Done

## Commits

- 28e3684 refactor: move PureWater acid-base list into data (ticket 02)

## Local Test Result

Baseline captured from the code before any edit of this ticket (ticket 01 files compiled in, unused), `gps_dump.in` (10 keV e-, `/run/beamOn 2`, `/chem/reaction/dump ReactionTable.txt`, `--dir`), then the same macro after the refactor:

```
exit=0
DUMP_SAME        (sorted ReactionTable.txt, baseline vs after: identical, 67 lines each,
                  including the "# Acid-base reactions" section, 20 lines)
EDEP_SAME        (20 keV in both)
success markers: 2, failure markers (EEEE|FatalException): 0
```

**Acceptance criterion not met as written: `PhysicsInteractions.csv` equal to baseline.** The premise (physics stage reproducible for a fixed seed) is false. The same binary gave different counts on repeated runs:

| run | Attachment | Elastic | ElectronSolvation | Excitation | Ionisation | VibExcitation |
|---|---|---|---|---|---|---|
| baseline (old code) | 21 | 49587 | 1008 | 128 | 1025 | 5522 |
| after02 | 19 | 49049 | 1010 | 125 | 1027 | 5630 |
| after02 rep1 | 21 | 49587 | 1008 | 128 | 1025 | 5522 |
| after02 rep2 | 23 | 47613 | 1009 | 126 | 1028 | 5438 |

Four runs, three distinct outcomes; rep1 coincidentally equals the baseline. The refactor cannot be the cause. `EnergyDeposit.Txt` is 20 keV in every run (both primaries deposit everything), so it does not discriminate either. Species yields are plausible and the same order of magnitude in both runs (at 1 ps: °OH 961 vs 970, H3O+ 795 vs 772, H2O2 26 vs 17).

The deterministic proof of "no behavior change" is therefore the reaction-table dump alone. Tickets 03 and 05 repeat the `PHYS_SAME` check; treat it as informational there too (compare magnitudes, not equality).

Unverified hypothesis: the RNG state carries from event 1's chemistry (non-deterministic per `sim.cc`) into event 2's physics, so event 2 differs run to run.

Ticket wording fix: the dump prints the bulk species as `H3O^1` / `OH^-1`, not `H3Op(B)`, so the baseline sanity check must look for the `# Acid-base reactions` section header, not a `(B)` name.

Logs and outputs: `.scratch/tests/2026-09-28__multiple-dna-chemistry/` (`baseline/`, `after02/`, `after02_rep1/`, `after02_rep2/`, `02-*.log`).

## Review Notes

- Values, order and reaction types of the 11 entries were compared line by line against the old `build(...)` calls; the dump match confirms them.
- Removed the now-unused `kOH`, `kHO2` and `<initializer_list>` from `DnaChemistryList.cc`; updated the file header and the header declaration comments.
- No compile warnings from the new brace-initialized list; no need for explicit element types.
- Only the four ticket files were staged. `compile_commands.json` and `.claude/hooks/hook-posttooluse.log` show as modified by a hook and were left out.

## Time Spent

~0.75 hours (mostly two 37 s runs plus the repeat runs and one build)

## Blockers / Challenges

None blocking. The physics-counts criterion is not reproducible by design of the simulation, documented above.

## Token Usage

- **Input:** 36
- **Output:** 13474
- **Cache read:** 3524182
- **Cache creation:** 24703
- **Total:** 3562395
